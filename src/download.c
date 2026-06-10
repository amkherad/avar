#include <cJSON.h>

#include <avar.h>
#include <config.h>
#include <utils.h>
#include <download.h>
#include <download_state.h>
#include <file-system.h>
#include <http.h>
#include <mongoose.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <debug.h>

#if defined(_WIN32)
    #include <direct.h>
#else
    #include <unistd.h>
#endif

#define DM_ITEMS_KEY "dm.items"
#define DL_CONNECT_TIMEOUT_MS 30000U
#define DL_IDLE_TIMEOUT_MS 120000U
#define DL_MAX_REDIRECTS 10
#define DL_WRITE_CHUNK_SIZE (1024U * 1024U)

typedef enum {
    DL_STEP_CHUNK,
    DL_STEP_STREAM,
} DlStep;

typedef struct {
    struct mg_mgr mgr;
    char *url;
    char *current_url;
    char *item_id;
    char *filename;
    char *temp_path;
    char *dest_path;
    char *state_path;
    DownloadState *state;
    FILE *fp;
    DlStep step;
    size_t active_chunk;
    uint64_t stream_received;
    uint64_t stream_expected;
    bool streaming;
    bool pending_schedule;
    bool schedule_deferred;
    bool request_sent;
    bool headers_received;
    bool use_ranges;
    int redirect_count;
    bool done;
    bool failed;
    char error[256];
    uint64_t connect_deadline_ms;
    uint64_t last_activity_ms;
    uint64_t last_progress_ms;
} DownloadJob;

static void set_error(DownloadJob *job, const char *fmt, ...);

static void close_file(DownloadJob *job);

static char *build_job_dir(const char *temp_dir, const char *item_id);

static char *build_state_path(const char *job_dir);

static char *format_datetime_iso(void);

static char *find_resumable_item_id(const char *url);

static uint64_t job_bytes_done(const DownloadJob *job);

static void sync_state_metadata(DownloadJob *job, const char *status);

static void remove_job_work_dir(const char *state_path);

static char *choose_filename(const char *url, const char *header_value, size_t header_len);

static char *build_item_json(const DownloadJob *job, const char *status);

static int dm_item_upsert(DownloadJob *job, const char *status);

static void print_progress(const DownloadJob *job);

static void report_cli(const DownloadJob *job);

static size_t next_chunk_index(const DownloadJob *job);

static uint64_t existing_file_size(const char *path);

static void send_request(DownloadJob *job, struct mg_connection *c, uint64_t range_start, uint64_t range_end);

static void send_request_for_step(DownloadJob *job, struct mg_connection *c);

static void init_download_tls(struct mg_connection *c, const char *url);

static bool fwrite_chunks(FILE *fp, const void *data, size_t len);

static bool open_temp_file(DownloadJob *job, const char *mode);

static bool write_at_offset(DownloadJob *job, const void *data, size_t len, uint64_t offset);

static bool append_stream(DownloadJob *job, const void *data, size_t len);

static void begin_stream_body(DownloadJob *job, struct mg_connection *c, struct mg_http_message *hm);

static bool store_header_value(char **dest, struct mg_str value);

static bool parse_content_range_total(struct mg_str value, uint64_t *total_out);

static bool apply_filename_from_headers(DownloadJob *job, struct mg_http_message *hm);

static bool finalize_download(DownloadJob *job);

static void request_close(DownloadJob *job, struct mg_connection *c, bool schedule_after_close);

static void schedule_next(DownloadJob *job);

static void flush_recv_stream(DownloadJob *job, struct mg_connection *c);

static bool try_parse_pending_response(DownloadJob *job, struct mg_connection *c);

static void handle_response_headers(DownloadJob *job, struct mg_connection *c,
                                    struct mg_http_message *hm);

static void on_connection_closed(DownloadJob *job, struct mg_connection *c);

static void dl_handler(struct mg_connection *c, int ev, void *ev_data);

static int run_download(DownloadJob *job);

static void job_free(DownloadJob *job);

static void set_error(DownloadJob *job, const char *fmt, ...) {
    if (job == NULL) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(job->error, sizeof job->error, fmt, ap);
    va_end(ap);
    job->failed = true;
    job->done = true;
    LOG_ERROR("%s", job->error);
}

static void close_file(DownloadJob *job) {
    if (job != NULL && job->fp != NULL) {
        fflush(job->fp);
        fclose(job->fp);
        job->fp = NULL;
    }
}

static char *build_job_dir(const char *temp_dir, const char *item_id) {
    return path_join(temp_dir, item_id);
}

static char *build_state_path(const char *job_dir) {
    return path_join(job_dir, DL_STATE_FILENAME);
}

static char *format_datetime_iso(void) {
    const time_t now = time(NULL);
    struct tm tm_utc;
#if defined(_WIN32)
    gmtime_s(&tm_utc, &now);
#else
    gmtime_r(&now, &tm_utc);
#endif
    char *buf = malloc(32);
    if (buf == NULL) {
        return NULL;
    }
    snprintf(buf, 32, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm_utc.tm_year + 1900,
             tm_utc.tm_mon + 1, tm_utc.tm_mday, tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return buf;
}

static char *find_resumable_item_id(const char *url) {
    const size_t count = get_config_array_size(DM_ITEMS_KEY);
    for (size_t i = 0; i < count; i++) {
        char *item_url = get_config_array_item_field(DM_ITEMS_KEY, i, "url");
        char *status = get_config_array_item_field(DM_ITEMS_KEY, i, "status");
        char *id = get_config_array_item_field(DM_ITEMS_KEY, i, "id");
        if (item_url != NULL && status != NULL && id != NULL && strcmp(item_url, url) == 0
            && strcmp(status, "completed") != 0) {
            char *result = strdup(id);
            free(item_url);
            free(status);
            free(id);
            return result;
        }
        free(item_url);
        free(status);
        free(id);
    }
    return NULL;
}

static uint64_t job_bytes_done(const DownloadJob *job) {
    if (job->state == NULL) {
        return 0;
    }
    if (job->step == DL_STEP_STREAM) {
        return job->stream_received;
    }
    return download_state_bytes_done(job->state);
}

static void sync_state_metadata(DownloadJob *job, const char *status) {
    if (job == NULL || job->state == NULL || job->state_path == NULL) {
        return;
    }

    free(job->state->status);
    job->state->status = status != NULL ? strdup(status) : NULL;
    job->state->bytes_downloaded = job_bytes_done(job);

    if (job->item_id != NULL) {
        free(job->state->id);
        job->state->id = strdup(job->item_id);
    }

    (void) download_state_save(job->state, job->state_path);
}

static void remove_job_work_dir(const char *state_path) {
    if (state_path == NULL) {
        return;
    }

    remove(state_path);

    const char *last_sep = strrchr(state_path, PATH_SEPARATOR);
    if (last_sep == NULL) {
        return;
    }

    char *job_dir = strndup(state_path, (size_t) (last_sep - state_path));
    if (job_dir == NULL) {
        return;
    }

#if defined(_WIN32)
    _rmdir(job_dir);
#else
    rmdir(job_dir);
#endif
    free(job_dir);
}

static char *choose_filename(const char *url, const char *header_value, const size_t header_len) {
    char *name = NULL;
    if (header_value != NULL && header_len > 0) {
        name = http_parse_content_disposition_filename(header_value, header_len);
    }
    if (name == NULL) {
        name = http_url_basename(url);
    }
    if (name == NULL) {
        const unsigned long stamp = (unsigned long) time(NULL);
        char fallback[64];
        snprintf(fallback, sizeof fallback, "download-%lu.bin", stamp);
        name = strdup(fallback);
    }

    char *sanitized = sanitize_filename(name);
    free(name);
    return sanitized;
}

static char *build_item_json(const DownloadJob *job, const char *status) {
    const uint64_t total = job->state != NULL ? job->state->total_size : 0;
    const uint64_t done_bytes = job_bytes_done(job);

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(obj, "id", job->item_id);
    cJSON_AddStringToObject(obj, "url", job->url);
    cJSON_AddStringToObject(obj, "filename", job->filename);
    cJSON_AddStringToObject(obj, "status", status);
    if (job->state != NULL && job->state->proxy != NULL) {
        cJSON_AddStringToObject(obj, "proxy", job->state->proxy);
    } else {
        cJSON_AddNullToObject(obj, "proxy");
    }
    cJSON_AddNumberToObject(obj, "bytesDownloaded", (double) done_bytes);
    cJSON_AddNumberToObject(obj, "totalBytes", (double) total);

    if (job->state != NULL && job->state->queued_at != NULL) {
        cJSON_AddStringToObject(obj, "queuedAt", job->state->queued_at);
    } else {
        cJSON_AddNullToObject(obj, "queuedAt");
    }
    if (job->state != NULL && job->state->last_try_at != NULL) {
        cJSON_AddStringToObject(obj, "lastTryAt", job->state->last_try_at);
    } else {
        cJSON_AddNullToObject(obj, "lastTryAt");
    }
    if (job->state != NULL && job->state->description != NULL) {
        cJSON_AddStringToObject(obj, "description", job->state->description);
    } else {
        cJSON_AddNullToObject(obj, "description");
    }
    if (job->state != NULL && job->state->original_page != NULL) {
        cJSON_AddStringToObject(obj, "originalPage", job->state->original_page);
    } else {
        cJSON_AddNullToObject(obj, "originalPage");
    }
    if (job->state != NULL && job->state->referer != NULL) {
        cJSON_AddStringToObject(obj, "referer", job->state->referer);
    } else {
        cJSON_AddNullToObject(obj, "referer");
    }
    cJSON_AddStringToObject(obj, "addedThrough",
                            job->state != NULL && job->state->added_through != NULL
                                ? job->state->added_through
                                : "direct");
    if (job->state != NULL && job->state->queue_id != NULL) {
        cJSON_AddStringToObject(obj, "queueId", job->state->queue_id);
    } else {
        cJSON_AddNullToObject(obj, "queueId");
    }

    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json;
}

static int dm_item_upsert(DownloadJob *job, const char *status) {
    sync_state_metadata(job, status);

    char *json = build_item_json(job, status);
    if (json == NULL) {
        return -1;
    }

    if (update_config_array_item(DM_ITEMS_KEY, "id", job->item_id, json) != 0) {
        const int rc = append_config_array_item(DM_ITEMS_KEY, json);
        cJSON_free(json);
        return rc;
    }

    cJSON_free(json);
    return 0;
}

static void print_progress(const DownloadJob *job) {
    const uint64_t total = job->state != NULL ? job->state->total_size : job->stream_expected;
    const uint64_t done_bytes = job_bytes_done(job);

    if (total > 0) {
        const double pct = (100.0 * (double) done_bytes) / (double) total;
        fprintf(stderr, "\r%s: %.1f%% (%llu / %llu bytes)", job->filename, pct,
                (unsigned long long) done_bytes, (unsigned long long) total);
    } else {
        fprintf(stderr, "\r%s: %llu bytes", job->filename, (unsigned long long) done_bytes);
    }
    fflush(stderr);
}

static void report_cli(const DownloadJob *job) {
    if (job->failed) {
        fprintf(stderr, "Download failed: %s\n", job->error[0] != '\0' ? job->error : "unknown error");
        return;
    }

    fprintf(stderr, "Download complete: %s\n", job->dest_path);
}

static size_t next_chunk_index(const DownloadJob *job) {
    if (job->state == NULL || job->state->chunks_done == NULL) {
        return 0;
    }

    for (size_t i = 0; i < job->state->chunk_count; i++) {
        if (!job->state->chunks_done[i]) {
            return i;
        }
    }
    return job->state->chunk_count;
}

static uint64_t existing_file_size(const char *path) {
    if (path == NULL) {
        return 0;
    }

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }

    return (uint64_t) st.st_size;
}

static void send_request(DownloadJob *job, struct mg_connection *c, const uint64_t range_start,
                         const uint64_t range_end) {
    const struct mg_str host = mg_url_host(job->current_url);
    const char *uri = mg_url_uri(job->current_url);

    LOG_DEBUG("Sending http request to %s", job->current_url);

    mg_printf(c,
              "GET %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: " APP_NAME "/" VERSION_STR "\r\n"
              "Accept: */*\r\n"
              "Accept-Encoding: identity\r\n"
              "Connection: close\r\n",
              uri, (int) host.len, host.buf);

    if (job->state != NULL && job->state->etag != NULL) {
        mg_printf(c, "If-Range: %s\r\n", job->state->etag);
    } else if (job->state != NULL && job->state->last_modified != NULL) {
        mg_printf(c, "If-Range: %s\r\n", job->state->last_modified);
    }

    if (job->step == DL_STEP_CHUNK) {
        mg_printf(c, "Range: bytes=%llu-%llu\r\n", (unsigned long long) range_start,
                  (unsigned long long) range_end);
    } else if (job->step == DL_STEP_STREAM && range_start > 0) {
        mg_printf(c, "Range: bytes=%llu-\r\n", (unsigned long long) range_start);
    }

    mg_printf(c, "\r\n");
}

static bool fwrite_chunks(FILE *fp, const void *data, const size_t len) {
    const unsigned char *cursor = data;
    size_t remaining = len;

    while (remaining > 0) {
        const size_t chunk =
                remaining > DL_WRITE_CHUNK_SIZE ? DL_WRITE_CHUNK_SIZE : remaining;
        if (fwrite(cursor, 1, chunk, fp) != chunk) {
            return false;
        }
        cursor += chunk;
        remaining -= chunk;
    }

    return true;
}

static void init_download_tls(struct mg_connection *c, const char *url) {
    struct mg_tls_opts opts = {0};
    opts.name = mg_url_host(url);
    mg_tls_init(c, &opts);

    if (c->tls == NULL || opts.name.len == 0) {
        return;
    }

    char host[256];
    const size_t host_len = opts.name.len < sizeof host - 1 ? opts.name.len : sizeof host - 1;
    memcpy(host, opts.name.buf, host_len);
    host[host_len] = '\0';

#if MG_TLS == MG_TLS_MBED
    struct mg_tls *tls = (struct mg_tls *) c->tls;
    if (mbedtls_ssl_set_hostname(&tls->ssl, host) != 0) {
        mg_error(c, "Failed to set TLS hostname");
    }
#else
    (void) host;
#endif
}

static void send_request_for_step(DownloadJob *job, struct mg_connection *c) {
    if (c->data[0] != '\0') {
        return;
    }

    c->data[0] = 1;
    job->request_sent = true;

    if (job->step == DL_STEP_CHUNK) {
        const uint64_t start = (uint64_t) job->active_chunk * (uint64_t) job->state->chunk_size;
        uint64_t end = start + (uint64_t) job->state->chunk_size - 1;
        if (job->state->total_size > 0 && end >= job->state->total_size) {
            end = job->state->total_size - 1;
        }
        send_request(job, c, start, end);
    } else if (job->step == DL_STEP_STREAM) {
        send_request(job, c, job->stream_received, 0);
    } else {
        send_request(job, c, 0, 0);
    }
}

static bool ensure_temp_dir(const char *temp_path) {
    const char *last_sep = strrchr(temp_path, PATH_SEPARATOR);
    if (last_sep == NULL) {
        return true;
    }

    char *dir = strndup(temp_path, (size_t) (last_sep - temp_path));
    if (dir == NULL) {
        return false;
    }

    const int rc = make_dirs_in_path(dir);
    free(dir);
    return rc == 0;
}

static bool open_temp_file(DownloadJob *job, const char *mode) {
    if (job->fp != NULL) {
        return true;
    }

    if (!ensure_temp_dir(job->temp_path)) {
        return false;
    }

    job->fp = fopen(job->temp_path, mode);

    LOG_DEBUG("Opened a temp file, path: %s", job->temp_path);

    return job->fp != NULL;
}

static bool write_at_offset(DownloadJob *job, const void *data, const size_t len,
                            const uint64_t offset) {
    if (job->fp == NULL) {
        if (!open_temp_file(job, "r+b") && !open_temp_file(job, "w+b")) {
            set_error(job, "Failed to open temp file: %s", job->temp_path);
            return false;
        }
    }

#if defined(_WIN32)
    if (_fseeki64(job->fp, (__int64) offset, SEEK_SET) != 0) {
#else
        if (fseeko(job->fp, (off_t) offset, SEEK_SET) != 0) {
#endif
        set_error(job, "Failed to seek in temp file");
        return false;
    }

    if (!fwrite_chunks(job->fp, data, len)) {
        set_error(job, "Failed to write temp file: %s", strerror(errno));
        return false;
    }

    fflush(job->fp);
    job->last_activity_ms = mg_millis();
    return true;
}

static bool append_stream(DownloadJob *job, const void *data, const size_t len) {
    if (len == 0) {
        return true;
    }

    LOG_DUMP("Writing stream to file, jobId: %s, len: %d", job->item_id, len);

    if (job->fp == NULL) {
        const char *mode = job->stream_received > 0 ? "ab" : "wb";
        if (!open_temp_file(job, mode)) {
            set_error(job, "Failed to open temp file: %s", job->temp_path);
            return false;
        }
    }

    if (!fwrite_chunks(job->fp, data, len)) {
        set_error(job, "Failed to write temp file: %s", strerror(errno));
        return false;
    }

    fflush(job->fp);
    job->stream_received += len;
    job->last_activity_ms = mg_millis();
    return true;
}

static size_t buffered_http_body(const struct mg_connection *c, const struct mg_http_message *hm) {
    if (c == NULL || hm == NULL || hm->body.len == 0 || c->recv.len <= hm->head.len) {
        return 0;
    }

    const size_t avail = c->recv.len - hm->head.len;
    return hm->body.len < avail ? hm->body.len : avail;
}

static void begin_stream_body(DownloadJob *job, struct mg_connection *c,
                              struct mg_http_message *hm) {
    job->streaming = true;
    c->pfn = NULL;

    const size_t to_write = buffered_http_body(c, hm);
    if (to_write > 0) {
        if (!append_stream(job, hm->body.buf, to_write)) {
            c->is_draining = 1;
            return;
        }
    }

    mg_iobuf_del(&c->recv, 0, hm->head.len + to_write);
    print_progress(job);
    (void) dm_item_upsert(job, "downloading");
}

static bool store_header_value(char **dest, const struct mg_str value) {
    if (dest == NULL || value.buf == NULL || value.len == 0) {
        return true;
    }

    char *copy = malloc(value.len + 1);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, value.buf, value.len);
    copy[value.len] = '\0';
    free(*dest);
    *dest = copy;
    return true;
}

static bool parse_content_range_total(const struct mg_str value, uint64_t *total_out) {
    if (total_out == NULL || value.buf == NULL) {
        return false;
    }

    const char *slash = memchr(value.buf, '/', value.len);
    if (slash == NULL) {
        return false;
    }

    const char *total_start = slash + 1;
    const size_t total_len = value.len - (size_t) (total_start - value.buf);
    char buffer[32];
    if (total_len >= sizeof buffer) {
        return false;
    }

    memcpy(buffer, total_start, total_len);
    buffer[total_len] = '\0';

    if (strcmp(buffer, "*") == 0) {
        return false;
    }

    char *end = NULL;
    const unsigned long long total = strtoull(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }

    *total_out = (uint64_t) total;
    return true;
}

static bool apply_filename_from_headers(DownloadJob *job, struct mg_http_message *hm) {
    struct mg_str *disposition = mg_http_get_header(hm, "Content-Disposition");
    if (disposition == NULL) {
        return true;
    }

    char *from_header =
            http_parse_content_disposition_filename(disposition->buf, disposition->len);
    if (from_header == NULL) {
        return true;
    }

    char *sanitized = sanitize_filename(from_header);
    free(from_header);
    if (sanitized == NULL) {
        return true;
    }

    free(job->filename);
    job->filename = sanitized;

    char *job_dir = NULL;
    if (job->state_path != NULL) {
        const char *last_sep = strrchr(job->state_path, PATH_SEPARATOR);
        if (last_sep != NULL) {
            job_dir = strndup(job->state_path, (size_t) (last_sep - job->state_path));
        }
    }

    char *download_dir = default_download_path();
    free(job->temp_path);
    free(job->dest_path);
    job->temp_path = job_dir != NULL ? path_join(job_dir, job->filename) : NULL;
    job->dest_path = download_dir != NULL ? path_join(download_dir, job->filename) : NULL;
    free(job_dir);
    free(download_dir);

    if (job->state != NULL) {
        free(job->state->filename);
        free(job->state->temp_path);
        free(job->state->dest_path);
        job->state->filename = strdup(job->filename);
        job->state->temp_path = job->temp_path != NULL ? strdup(job->temp_path) : NULL;
        job->state->dest_path = job->dest_path != NULL ? strdup(job->dest_path) : NULL;
    }

    return job->temp_path != NULL && job->dest_path != NULL && job->state_path != NULL;
}

static bool finalize_download(DownloadJob *job) {
    LOG_DEBUG("Finalizing download, jobId: %s", job->item_id);

    if (job->done) {
        return !job->failed;
    }

    close_file(job);

    if (job->state != NULL && job->state->total_size > 0) {
        const uint64_t done = job->step == DL_STEP_STREAM
                                  ? job->stream_received
                                  : download_state_bytes_done(job->state);
        if (done < job->state->total_size) {
            set_error(job, "Download incomplete (%llu / %llu bytes)",
                      (unsigned long long) done, (unsigned long long) job->state->total_size);
            return false;
        }
    }

    const char *last_sep = strrchr(job->dest_path, PATH_SEPARATOR);
    if (last_sep != NULL) {
        char *dir = strndup(job->dest_path, (size_t) (last_sep - job->dest_path));
        if (dir != NULL) {
            (void) make_dirs_in_path(dir);
            free(dir);
        }
    }

    if (move_file_atomic(job->temp_path, job->dest_path) != 0) {
        set_error(job, "Failed to move file to %s", job->dest_path);
        return false;
    }

    (void) dm_item_upsert(job, "completed");
    remove_job_work_dir(job->state_path);
    print_progress(job);
    job->done = true;
    return true;
}

static void request_close(DownloadJob *job, struct mg_connection *c, const bool schedule_after_close) {
    job->pending_schedule = schedule_after_close;
    c->is_draining = 1;
}

static void flush_recv_stream(DownloadJob *job, struct mg_connection *c) {
    if (job == NULL || c == NULL || !job->streaming || c->recv.len == 0) {
        return;
    }

    if (!append_stream(job, c->recv.buf, c->recv.len)) {
        return;
    }

    c->recv.len = 0;
}

static bool try_parse_pending_response(DownloadJob *job, struct mg_connection *c) {
    if (job->headers_received || c == NULL || c->recv.len == 0) {
        return false;
    }

    struct mg_http_message hm;
    const int n = mg_http_parse((char *) c->recv.buf, c->recv.len, &hm);
    if (n <= 0) {
        return false;
    }

    job->headers_received = true;
    handle_response_headers(job, c, &hm);
    return true;
}

static void on_connection_closed(DownloadJob *job, struct mg_connection *c) {
    if (job == NULL || job->done || job->failed) {
        return;
    }

    LOG_DEBUG("Connection closed, step=%d streaming=%d pending_schedule=%d headers_received=%d",
              (int) job->step, job->streaming, job->pending_schedule, job->headers_received);

    if (c != NULL && !job->headers_received) {
        (void) try_parse_pending_response(job, c);
    }

    flush_recv_stream(job, c);

    if (job->pending_schedule) {
        job->pending_schedule = false;
        job->schedule_deferred = true;
        return;
    }

    if (job->step == DL_STEP_STREAM && (job->streaming || job->stream_received > 0)) {
        if (job->stream_expected == 0 || job->stream_received >= job->stream_expected) {
            (void) finalize_download(job);
        } else {
            set_error(job, "Connection closed before download completed (%llu / %llu bytes)",
                      (unsigned long long) job->stream_received,
                      (unsigned long long) job->stream_expected);
            (void) dm_item_upsert(job, "failed");
        }
        return;
    }

    if (job->step == DL_STEP_CHUNK) {
        if (download_state_all_chunks_done(job->state)) {
            (void) finalize_download(job);
        } else {
            set_error(job, "Connection closed before all chunks were downloaded");
            (void) dm_item_upsert(job, "failed");
        }
        return;
    }

    if (!job->request_sent) {
        if (job->redirect_count > 0) {
            set_error(job, "Connection failed while following redirect to %s",
                      job->current_url != NULL ? job->current_url : job->url);
        } else {
            set_error(job, "Failed to send download request");
        }
    } else {
        set_error(job, "Connection closed before the download started");
    }
    (void) dm_item_upsert(job, "failed");
}

static void schedule_next(DownloadJob *job) {
    LOG_DEBUG("Scheduling a job, jobId: %s", job->item_id);

    if (job->failed || job->done) {
        return;
    }

    job->headers_received = false;
    job->request_sent = false;
    job->streaming = false;

    if (job->step == DL_STEP_CHUNK) {
        const size_t chunk = next_chunk_index(job);
        if (chunk >= job->state->chunk_count) {
            (void) finalize_download(job);
            return;
        }
        job->active_chunk = chunk;
    }

    struct mg_connection *c = mg_http_connect(&job->mgr, job->current_url, dl_handler, job);
    if (c == NULL) {
        set_error(job, "Failed to connect to %s", job->current_url);
    }
}

static bool handle_redirect(DownloadJob *job, struct mg_connection *c,
                            struct mg_http_message *hm) {
    const int status = mg_http_status(hm);
    if (!http_is_redirect(status)) {
        return false;
    }

    struct mg_str *location = mg_http_get_header(hm, "Location");
    if (location == NULL) {
        set_error(job, "Redirect without Location header");
        request_close(job, c, false);
        return true;
    }

    if (job->redirect_count >= DL_MAX_REDIRECTS) {
        set_error(job, "Too many redirects");
        request_close(job, c, false);
        return true;
    }

    char *resolved = http_resolve_url(job->current_url, location->buf, location->len);
    if (resolved == NULL) {
        set_error(job, "Invalid redirect location");
        request_close(job, c, false);
        return true;
    }

    free(job->current_url);
    job->current_url = resolved;
    job->redirect_count++;
    LOG_DEBUG("Following redirect (%d) to %s", job->redirect_count, job->current_url);
    job->headers_received = false;
    job->request_sent = false;
    job->streaming = false;
    request_close(job, c, true);
    return true;
}

static void handle_response_headers(DownloadJob *job, struct mg_connection *c,
                                    struct mg_http_message *hm) {
    job->headers_received = true;
    const int status = mg_http_status(hm);

    if (handle_redirect(job, c, hm)) {
        return;
    }

    if (status == 416) {
        if (job->step == DL_STEP_CHUNK && download_state_all_chunks_done(job->state)) {
            (void) finalize_download(job);
        } else {
            set_error(job, "Server rejected range request");
        }
        request_close(job, c, false);
        return;
    }

    if (status < 200 || status >= 300) {
        set_error(job, "HTTP error %d", status);
        request_close(job, c, false);
        return;
    }

    if (!apply_filename_from_headers(job, hm)) {
        set_error(job, "Failed to apply download filename");
        request_close(job, c, false);
        return;
    }

    struct mg_str *etag = mg_http_get_header(hm, "ETag");
    if (etag != NULL) {
        (void) store_header_value(&job->state->etag, *etag);
    }

    struct mg_str *last_modified = mg_http_get_header(hm, "Last-Modified");
    if (last_modified != NULL) {
        (void) store_header_value(&job->state->last_modified, *last_modified);
    }

    struct mg_str *accept_ranges = mg_http_get_header(hm, "Accept-Ranges");
    if (accept_ranges != NULL) {
        job->use_ranges = mg_strcasecmp(*accept_ranges, mg_str("none")) != 0;
    }

    struct mg_str *content_range = mg_http_get_header(hm, "Content-Range");
    if (content_range != NULL && job->state->total_size == 0) {
        (void) parse_content_range_total(*content_range, &job->state->total_size);
    }

    struct mg_str *content_length = mg_http_get_header(hm, "Content-Length");
    if (content_length != NULL && content_length->len < 32) {
        char buffer[32];
        memcpy(buffer, content_length->buf, content_length->len);
        buffer[content_length->len] = '\0';
        const uint64_t len = strtoull(buffer, NULL, 10);

        if (job->step == DL_STEP_STREAM && job->stream_expected == 0) {
            job->stream_expected = job->stream_received + len;
            if (job->state->total_size == 0) {
                job->state->total_size = job->stream_expected;
            }
        }
    }

    if (job->step == DL_STEP_STREAM) {
        begin_stream_body(job, c, hm);
    }
}

static void dl_handler(struct mg_connection *c, int ev, void *ev_data) {
    DownloadJob *job = (DownloadJob *) c->fn_data;
    if (job == NULL || job->done) {
        return;
    }

    if (ev == MG_EV_POLL) {
        // Check this early to improve performance
        if (mg_millis() > job->connect_deadline_ms && (c->is_connecting || c->is_resolving)) {
            mg_error(c, "Connect timeout");

            LOG_ERROR("Connect timeout, jobId: %s", job->item_id);
        }

        if (!job->failed && mg_millis() - job->last_activity_ms > DL_IDLE_TIMEOUT_MS) {
            mg_error(c, "Download stalled");

            LOG_ERROR("Download stalled, jobId: %s", job->item_id);
        }

        // return;
    }

    LOG_DUMP("Mongoose event '%s' (%d) was received", mg_event_message(ev), ev);

    if (ev == MG_EV_RESOLVE) {
        // LOG_INFO(
        //     "Data: %d.%d.%d.%d (%d) (%d)",
        //     (c->loc.addr.ip4 & 0xff),
        //     (c->loc.addr.ip4 & 0xff00) >> 8,
        //     (c->loc.addr.ip4 & 0xff0000) >> 16,
        //     (c->loc.addr.ip4 & 0xff000000) >> 24,
        //     c->loc.addr.ip4,
        //     c->loc.addr.ip6
        // );
    } else if (ev == MG_EV_OPEN) {
        //LOG_DUMP("Mongoose MG_EV_OPEN");

        job->connect_deadline_ms = mg_millis() + DL_CONNECT_TIMEOUT_MS;
        job->last_activity_ms = mg_millis();
    } else if (ev == MG_EV_CONNECT) {
        c->data[0] = '\0';
        job->headers_received = false;
        job->request_sent = false;
        job->streaming = false;
        if (c->is_tls) {
            const struct mg_str host = mg_url_host(job->current_url);
            LOG_DEBUG("Starting TLS to %.*s", (int) host.len,
                      host.buf != NULL ? host.buf : "");
            init_download_tls(c, job->current_url);
        } else {
            send_request_for_step(job, c);
        }
    } else if (ev == MG_EV_TLS_HS) {
        send_request_for_step(job, c);
    } else if (ev == MG_EV_WRITE) {
        if (!job->request_sent && !c->is_connecting && !c->is_tls_hs) {
            send_request_for_step(job, c);
        }
    } else if (ev == MG_EV_HTTP_HDRS) {
        handle_response_headers(job, c, (struct mg_http_message *) ev_data);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        if (job->step == DL_STEP_STREAM && job->streaming) {
            return;
        }

        if (job->step != DL_STEP_CHUNK) {
            return;
        }

        if (hm->body.len == 0) {
            set_error(job, "Empty chunk response for chunk %zu", job->active_chunk);
            request_close(job, c, false);
            return;
        }

        const uint64_t offset =
                (uint64_t) job->active_chunk * (uint64_t) job->state->chunk_size;
        if (!write_at_offset(job, hm->body.buf, hm->body.len, offset)) {
            request_close(job, c, false);
            return;
        }

        job->state->chunks_done[job->active_chunk] = true;
        (void) dm_item_upsert(job, "downloading");
        print_progress(job);

        if (download_state_all_chunks_done(job->state)) {
            request_close(job, c, false);
            (void) finalize_download(job);
        } else {
            request_close(job, c, true);
        }
    } else if (ev == MG_EV_READ) {
        if (job->streaming && c->recv.len > 0) {
            const size_t len = c->recv.len;
            if (!append_stream(job, c->recv.buf, len)) {
                request_close(job, c, false);
                return;
            }
            c->recv.len = 0;

            if (mg_millis() - job->last_progress_ms >= 200) {
                print_progress(job);
                (void) dm_item_upsert(job, "downloading");
                job->last_progress_ms = mg_millis();
            }
        }
    } else if (ev == MG_EV_CLOSE) {
        on_connection_closed(job, c);
    } else if (ev == MG_EV_ERROR) {
        if (!job->done) {
            const char *message = (const char *) ev_data;
            LOG_DEBUG("Connection error on %s: %s", job->current_url != NULL ? job->current_url : job->url,
                      message != NULL && message[0] != '\0' ? message : "(none)");
            if (message != NULL && message[0] != '\0') {
                set_error(job, "%s", message);
            } else {
                set_error(job, "Network error");
            }
            (void) dm_item_upsert(job, "failed");
        }
    }
}

static int run_download(DownloadJob *job) {
    mg_mgr_init(&job->mgr);
    mg_log_set(MG_LL_ERROR);

    job->stream_received = existing_file_size(job->temp_path);
    if (job->step == DL_STEP_STREAM && job->stream_received > 0 && job->state->total_size == 0
        && download_state_bytes_done(job->state) == 0) {
        remove(job->temp_path);
        job->stream_received = 0;
    }
    job->last_activity_ms = mg_millis();
    job->last_progress_ms = mg_millis();

    if (job->state->total_size > 0 && job->state->chunk_count > 0
        && !download_state_all_chunks_done(job->state)) {
        job->step = DL_STEP_CHUNK;
        job->active_chunk = next_chunk_index(job);
    } else {
        job->step = DL_STEP_STREAM;
    }

    job->current_url = strdup(job->url);
    if (job->current_url == NULL) {
        set_error(job, "Out of memory");
        return EXIT_FAILURE;
    }

    LOG_DEBUG("Scheduling download job, jobId: %s", job->item_id);
    schedule_next(job);
    while (!job->done && !job->failed) {
        mg_mgr_poll(&job->mgr, 50);
        if (job->schedule_deferred) {
            job->schedule_deferred = false;
            schedule_next(job);
        }
    }
    LOG_DEBUG("Finished download job, jobId: %s", job->item_id);

    fputc('\n', stderr);
    report_cli(job);
    mg_mgr_free(&job->mgr);
    return job->failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void job_free(DownloadJob *job) {
    if (job == NULL) {
        return;
    }

    close_file(job);
    free(job->url);
    free(job->current_url);
    free(job->item_id);
    free(job->filename);
    free(job->temp_path);
    free(job->dest_path);
    free(job->state_path);
    download_state_free(job->state);
    free(job);
}

int transient_download(const char *url, const char *queue, const char *name, const bool attached) {
    (void) name;

    if (!attached) {
        LOG_ERROR("Only attached downloads are supported in this version");
        return EXIT_FAILURE;
    }

    if (url == NULL || !is_valid_http_url(url)) {
        LOG_ERROR("Invalid download URL");
        return EXIT_FAILURE;
    }

    char *temp_dir = default_temp_path();
    char *download_dir = default_download_path();
    if (temp_dir == NULL || download_dir == NULL) {
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to resolve download directories");
        return EXIT_FAILURE;
    }

    if (make_dirs_in_path(temp_dir) != 0 || make_dirs_in_path(download_dir) != 0) {
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to create download directories");
        return EXIT_FAILURE;
    }

    char *item_id = find_resumable_item_id(url);
    if (item_id == NULL) {
        char generated[64];
        snprintf(generated, sizeof generated, "dl-%llu", (unsigned long long) time(NULL));
        item_id = strdup(generated);
    }

    if (item_id == NULL) {
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to allocate download id");
        return EXIT_FAILURE;
    }

    char *job_dir = build_job_dir(temp_dir, item_id);
    char *state_path = job_dir != NULL ? build_state_path(job_dir) : NULL;
    if (job_dir == NULL || state_path == NULL || make_dirs_in_path(job_dir) != 0) {
        free(item_id);
        free(job_dir);
        free(state_path);
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to create download work directory");
        return EXIT_FAILURE;
    }

    DownloadState *state = download_state_load(state_path);
    char *filename = NULL;
    char *temp_path = NULL;
    char *dest_path = NULL;

    if (state != NULL && strcmp(state->url, url) != 0) {
        download_state_free(state);
        state = NULL;
        remove(state_path);
    }

    if (state != NULL) {
        filename = strdup(state->filename);
        temp_path = strdup(state->temp_path);
        dest_path = strdup(state->dest_path);
        if (state->id == NULL) {
            state->id = strdup(item_id);
        }
        if (state->queued_at == NULL) {
            state->queued_at = format_datetime_iso();
        }
        if (state->added_through == NULL) {
            state->added_through = strdup("direct");
        }
        if (state->id != NULL) {
            free(item_id);
            item_id = strdup(state->id);
        }
    } else {
        filename = choose_filename(url, NULL, 0);
        if (filename == NULL) {
            free(item_id);
            free(job_dir);
            free(state_path);
            free(temp_dir);
            free(download_dir);
            LOG_ERROR("Failed to determine filename");
            return EXIT_FAILURE;
        }
        temp_path = path_join(job_dir, filename);
        dest_path = path_join(download_dir, filename);
        state = download_state_create(url, filename, temp_path, dest_path, 0, DL_CHUNK_SIZE);
        if (state != NULL) {
            state->id = strdup(item_id);
            state->queued_at = format_datetime_iso();
            state->added_through = strdup("direct");
            if (queue != NULL) {
                free(state->queue_id);
                state->queue_id = strdup(queue);
            }
        }
    }

    free(temp_dir);
    free(download_dir);
    free(job_dir);

    if (state == NULL || filename == NULL || temp_path == NULL || dest_path == NULL) {
        download_state_free(state);
        free(item_id);
        free(filename);
        free(temp_path);
        free(dest_path);
        free(state_path);
        LOG_ERROR("Failed to initialize download state");
        return EXIT_FAILURE;
    }

    DownloadJob *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        download_state_free(state);
        free(item_id);
        free(filename);
        free(temp_path);
        free(dest_path);
        free(state_path);
        LOG_ERROR("Out of memory");
        return EXIT_FAILURE;
    }

    char *last_try_at = format_datetime_iso();
    if (last_try_at != NULL) {
        free(state->last_try_at);
        state->last_try_at = last_try_at;
    }

    job->url = strdup(url);
    job->item_id = item_id;
    job->filename = filename;
    job->temp_path = temp_path;
    job->dest_path = dest_path;
    job->state_path = state_path;
    job->state = state;
    job->use_ranges = true;
    job->last_progress_ms = mg_millis();

    LOG_INFO("Downloading %s <- %s", dest_path, url);
    (void) dm_item_upsert(job, "downloading");

    const int rc = run_download(job);
    if (rc != EXIT_SUCCESS && !job->failed) {
        (void) dm_item_upsert(job, "failed");
    }
    job_free(job);
    return rc;
}
