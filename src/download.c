#include <cJSON.h>

#include <avar.h>
#include <config.h>
#include <utils.h>
#include <download.h>
#include <download_state.h>
#include <download_config.h>
#include <download_segment.h>
#include <download_sync.h>
#include <thread_pool.h>
#include <file-system.h>
#include <queue.h>
#include <http.h>
#include <mongoose.h>

#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <debug.h>
#include <logger.h>

#if defined(_WIN32)
    #include <direct.h>
    #include <io.h>
    #include <process.h>
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

static atomic_uint g_active_downloads = 0;

typedef struct {
    char *url;
    char *queue;
    char *name;
} BackgroundDownloadArgs;

size_t download_active_count(void) {
    return (size_t)atomic_load(&g_active_downloads);
}

size_t download_active_list(DownloadActiveInfo *items, const size_t max_items) {
    size_t written = 0U;
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_STATUS);
        if (status == NULL || strcmp(status, AVAR_DL_STATUS_DOWNLOADING) != 0) {
            free(status);
            continue;
        }
        free(status);

        if (items != NULL && written < max_items) {
            DownloadActiveInfo *out = &items[written];
            memset(out, 0, sizeof(*out));

            char *id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_ID);
            char *filename = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_FILENAME);
            char *done_str =
                get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_BYTES_DOWNLOADED);
            char *total_str = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_TOTAL_BYTES);

            if (id != NULL) {
                snprintf(out->id, sizeof out->id, "%s", id);
            }
            if (filename != NULL) {
                snprintf(out->filename, sizeof out->filename, "%s", filename);
            }
            if (done_str != NULL) {
                out->bytes_downloaded = strtoull(done_str, NULL, 10);
            }
            if (total_str != NULL) {
                out->total_bytes = strtoull(total_str, NULL, 10);
            }

            free(id);
            free(filename);
            free(done_str);
            free(total_str);
        }

        written++;
    }
    return written;
}

bool download_wait_idle(const unsigned timeout_ms) {
    const unsigned step_ms = 100U;
    for (unsigned elapsed = 0U; elapsed < timeout_ms; elapsed += step_ms) {
        if (download_active_count() == 0U) {
            return true;
        }
#if defined(_WIN32)
        Sleep(step_ms);
#else
        usleep((useconds_t)step_ms * 1000U);
#endif
    }
    return download_active_count() == 0U;
}

typedef enum {
    DL_STEP_CHUNK,
    DL_STEP_STREAM,
} DlStep;

typedef struct DownloadJob DownloadJob;

typedef struct ChunkSlot {
    struct mg_connection *conn;
    size_t chunk_index;
    uint64_t chunk_write_offset;
    uint64_t chunk_received;
    uint64_t chunk_expected;
    bool headers_received;
    bool streaming;
    bool request_sent;
    bool in_use;
} ChunkSlot;

typedef struct DlConnCtx {
    DownloadJob *job;
    ChunkSlot *slot;
} DlConnCtx;

typedef struct DownloadJob {
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
    AvarMutex *mutex;
    ChunkSlot *slots;
    size_t slot_capacity;
    DownloadSegmentConfig seg_cfg;
    bool segment_mode;
    DlStep step;
    size_t active_chunk;
    uint64_t chunk_write_offset;
    uint64_t chunk_received;
    uint64_t chunk_expected;
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
    char error[AVAR_DL_ERROR_BUF_SIZE];
    uint64_t connect_deadline_ms;
    uint64_t last_activity_ms;
    uint64_t last_progress_ms;
    uint64_t last_speed_sample_ms;
    uint64_t last_speed_bytes;
    double last_speed_bps;
    uint64_t last_progress_total;
    int last_progress_total_num_width;
    size_t progress_line_max_len;
    bool last_progress_had_total;
    int last_progress_bar_width;
} DownloadJob;

static void set_error(DownloadJob *job, const char *fmt, ...);

static void close_file(DownloadJob *job);

static void sync_temp_file(DownloadJob *job);

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

static void print_progress(DownloadJob *job);

static void report_cli(const DownloadJob *job);

static size_t next_chunk_index(const DownloadJob *job);

static uint64_t existing_file_size(const char *path);

static void send_request(DownloadJob *job, struct mg_connection *c, uint64_t range_start, uint64_t range_end);

static void send_request_for_slot(DownloadJob *job, struct mg_connection *c, ChunkSlot *slot);

static void init_download_tls(struct mg_connection *c, const char *url);

static bool fwrite_chunks(FILE *fp, const void *data, size_t len);

static bool open_temp_file(DownloadJob *job, const char *mode);

static bool write_at_offset(DownloadJob *job, const void *data, size_t len, uint64_t offset);

static bool append_stream(DownloadJob *job, const void *data, size_t len);

// static void sync_temp_file(DownloadJob *job);

static void begin_stream_body(DownloadJob *job, struct mg_connection *c, struct mg_http_message *hm);

static void begin_chunk_body(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c,
                             struct mg_http_message *hm);

static bool complete_chunk_slot(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c);

static bool store_header_value(char **dest, struct mg_str value);

static bool parse_content_range_total(struct mg_str value, uint64_t *total_out);

static bool apply_filename_from_headers(DownloadJob *job, struct mg_http_message *hm);

static bool finalize_download(DownloadJob *job);

static void request_close(DownloadJob *job, struct mg_connection *c, bool schedule_after_close);

static void schedule_pending_chunks(DownloadJob *job);

static DlConnCtx *dl_conn_ctx_create(DownloadJob *job, ChunkSlot *slot);

static void dl_conn_ctx_free(struct mg_connection *c);

static ChunkSlot *chunk_slot_find(DownloadJob *job, const struct mg_connection *c);

static ChunkSlot *chunk_slot_acquire(DownloadJob *job, size_t chunk_index);

static void chunk_slot_release(DownloadJob *job, ChunkSlot *slot);

static size_t count_active_slots(const DownloadJob *job);

static bool chunk_in_flight_predicate(const void *ctx, size_t chunk_index);

static void disable_segment_mode(DownloadJob *job);

static bool try_enable_segment_mode(DownloadJob *job);

static void schedule_next(DownloadJob *job);

static void flush_recv_body(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c);

static bool try_parse_pending_response(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c);

static void handle_response_headers(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c,
                                    struct mg_http_message *hm);

static void on_connection_closed(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c);

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
        sync_temp_file(job);
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
    char *buf = malloc(AVAR_DATETIME_BUF_SIZE);
    if (buf == NULL) {
        return NULL;
    }
    snprintf(buf, AVAR_DATETIME_BUF_SIZE, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm_utc.tm_year + 1900,
             tm_utc.tm_mon + 1, tm_utc.tm_mday, tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return buf;
}

static char *find_resumable_item_id(const char *url) {
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; i++) {
        char *item_url = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_URL);
        char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_STATUS);
        char *id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_ID);
        if (item_url != NULL && status != NULL && id != NULL && strcmp(item_url, url) == 0
            && strcmp(status, AVAR_DL_STATUS_COMPLETED) != 0) {
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

    uint64_t bytes = download_state_bytes_done(job->state);
    if (job->slots != NULL) {
        for (size_t i = 0; i < job->slot_capacity; i++) {
            if (job->slots[i].in_use && job->slots[i].streaming) {
                bytes += job->slots[i].chunk_received;
            }
        }
    } else if (job->streaming) {
        bytes += job->chunk_received;
    }
    return bytes;
}

static void sync_state_metadata(DownloadJob *job, const char *status) {
    if (job == NULL || job->state == NULL || job->state_path == NULL) {
        return;
    }

    if (job->mutex != NULL) {
        avar_mutex_lock(job->mutex);
    }

    free(job->state->status);
    job->state->status = status != NULL ? strdup(status) : NULL;
    job->state->bytes_downloaded = job_bytes_done(job);

    if (job->item_id != NULL) {
        free(job->state->id);
        job->state->id = strdup(job->item_id);
    }

    (void)download_state_save(job->state, job->state_path);

    if (job->mutex != NULL) {
        avar_mutex_unlock(job->mutex);
    }
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
        char fallback[AVAR_DL_FALLBACK_NAME_BUF_SIZE];
        snprintf(fallback, sizeof fallback, "%s%lu%s", AVAR_DOWNLOAD_FALLBACK_PREFIX, stamp,
                 AVAR_DOWNLOAD_FALLBACK_SUFFIX);
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

    cJSON_AddStringToObject(obj, AVAR_FIELD_ID, job->item_id);
    cJSON_AddStringToObject(obj, AVAR_FIELD_URL, job->url);
    cJSON_AddStringToObject(obj, AVAR_FIELD_FILENAME, job->filename);
    cJSON_AddStringToObject(obj, AVAR_FIELD_STATUS, status);
    if (job->state != NULL && job->state->proxy != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_PROXY, job->state->proxy);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_PROXY);
    }
    cJSON_AddNumberToObject(obj, AVAR_FIELD_BYTES_DOWNLOADED, (double) done_bytes);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_TOTAL_BYTES, (double) total);

    if (job->state != NULL && job->state->queued_at != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_QUEUED_AT, job->state->queued_at);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_QUEUED_AT);
    }
    if (job->state != NULL && job->state->last_try_at != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_LAST_TRY_AT, job->state->last_try_at);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_LAST_TRY_AT);
    }
    if (job->state != NULL && job->state->description != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_DESCRIPTION, job->state->description);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_DESCRIPTION);
    }
    if (job->state != NULL && job->state->original_page != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_ORIGINAL_PAGE, job->state->original_page);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_ORIGINAL_PAGE);
    }
    if (job->state != NULL && job->state->referer != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_REFERER, job->state->referer);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_REFERER);
    }
    cJSON_AddStringToObject(obj, AVAR_FIELD_ADDED_THROUGH,
                            job->state != NULL && job->state->added_through != NULL
                                ? job->state->added_through
                                : AVAR_DL_ADDED_DIRECT);
    if (job->state != NULL && job->state->queue_id != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_QUEUE_ID, job->state->queue_id);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_QUEUE_ID);
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

    if (update_config_array_item(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, job->item_id, json) != 0) {
        const int rc = append_config_array_item(AVAR_CFG_DM_ITEMS, json);
        cJSON_free(json);
        return rc;
    }

    cJSON_free(json);
    return 0;
}

static void clear_progress_line(DownloadJob *job) {
    if (job == NULL || job->progress_line_max_len == 0) {
        return;
    }

#if defined(_WIN32)
    static bool vt_enabled = false;
    if (!vt_enabled && avar_stderr_is_tty()) {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        vt_enabled = true;
    }
#endif

    if (avar_stderr_is_tty()) {
        fputs("\033[2K\r", stderr);
    } else {
        fprintf(stderr, "\r%*s\r", (int) job->progress_line_max_len, "");
    }
    job->progress_line_max_len = 0;
    fflush(stderr);
}

static void write_progress_line(DownloadJob *job, const char *line) {
    const int cols = avar_terminal_columns();
    size_t len = strlen(line);
    if (cols > 0 && len > (size_t) cols) {
        len = (size_t) cols;
    }

#if defined(_WIN32)
    static bool vt_enabled = false;
    if (!vt_enabled && avar_stderr_is_tty()) {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        vt_enabled = true;
    }
#endif

    if (avar_stderr_is_tty()) {
        fputs("\033[2K\r", stderr);
    } else {
        fputs("\r", stderr);
    }

    fwrite(line, 1, len, stderr);

    const size_t pad_to = job->progress_line_max_len;
    if (len < pad_to) {
        for (size_t i = len; i < pad_to; ++i) {
            fputc(' ', stderr);
        }
    } else if (len > pad_to) {
        job->progress_line_max_len = len;
    }

    fflush(stderr);
}

static int progress_bar_equals_count(const char *bar) {
    if (bar == NULL || bar[0] != '[') {
        return 0;
    }

    int count = 0;
    for (const char *p = bar + 1; *p != '\0' && *p != ']'; ++p) {
        if (*p == '=') {
            count++;
        }
    }

    return count;
}

static bool column_overlaps_range(const uint64_t col_start, const uint64_t col_end,
                                  const uint64_t range_start, const uint64_t range_end) {
    return col_start < range_end && range_start < col_end;
}

static bool progress_column_filled(const uint64_t col_start, const uint64_t col_end, void *ctx) {
    const DownloadJob *job = (const DownloadJob *) ctx;
    if (job == NULL || col_start >= col_end) {
        return false;
    }

    if (job->state != NULL && job->state->chunks_done != NULL) {
        for (size_t i = 0; i < job->state->chunk_count; i++) {
            if (!job->state->chunks_done[i]) {
                continue;
            }

            uint64_t start = 0;
            uint64_t end = 0;
            segment_chunk_range(job->state, i, &start, &end);
            if (column_overlaps_range(col_start, col_end, start, end + 1U)) {
                return true;
            }
        }
    }

    if (job->slots != NULL && job->state != NULL) {
        for (size_t i = 0; i < job->slot_capacity; i++) {
            const ChunkSlot *slot = &job->slots[i];
            if (!slot->in_use || !slot->streaming || slot->chunk_received == 0) {
                continue;
            }

            const uint64_t recv_end = slot->chunk_write_offset + slot->chunk_received;
            if (column_overlaps_range(col_start, col_end, slot->chunk_write_offset, recv_end)) {
                return true;
            }
        }
    } else if (job->step == DL_STEP_CHUNK && job->streaming && job->chunk_received > 0) {
        const uint64_t start = job->chunk_write_offset;
        const uint64_t end = job->chunk_write_offset + job->chunk_received;
        if (column_overlaps_range(col_start, col_end, start, end)) {
            return true;
        }
    } else if (job->step == DL_STEP_STREAM && job->stream_received > 0) {
        if (column_overlaps_range(col_start, col_end, 0, job->stream_received)) {
            return true;
        }
    }

    return false;
}

static void print_progress(DownloadJob *job) {
    const uint64_t total = job->state != NULL ? job->state->total_size : job->stream_expected;
    const uint64_t done_bytes = job_bytes_done(job);
    const uint64_t now = mg_millis();

    if (job->last_speed_sample_ms == 0) {
        job->last_speed_sample_ms = now;
        job->last_speed_bytes = done_bytes;
    } else if (now > job->last_speed_sample_ms) {
        const double elapsed = (now - job->last_speed_sample_ms) / 1000.0;
        if (elapsed >= 0.2) {
            job->last_speed_bps = (double) (done_bytes - job->last_speed_bytes) / elapsed;
            job->last_speed_bytes = done_bytes;
            job->last_speed_sample_ms = now;
        }
    }

    AvarSizeUnit size_unit = AVAR_SIZE_MIB;
    AvarSpeedUnit speed_unit = AVAR_SPEED_MIBIT_PER_SEC;
    char *size_cfg = get_config_or_default(AVAR_CFG_DM_PROGRESS_SIZE_UNIT, AVAR_DEFAULT_SIZE_UNIT);
    char *speed_cfg =
            get_config_or_default(AVAR_CFG_DM_PROGRESS_SPEED_UNIT, AVAR_DEFAULT_SPEED_UNIT);
    (void) avar_size_unit_parse(size_cfg, &size_unit);
    (void) avar_speed_unit_parse(speed_cfg, &speed_unit);
    free(size_cfg);
    free(speed_cfg);

    int percent = 0;
    if (total > 0) {
        percent = (int) ((100.0 * (double) done_bytes) / (double) total);
        if (percent > 100) {
            percent = 100;
        }
    }

    const bool has_total = total > 0;
    const int total_num_width =
            has_total ? avar_data_size_number_width(total, size_unit) : 0;
    const int done_num_width = avar_data_size_number_width(done_bytes, size_unit);

    if (has_total && job->last_progress_total > 0 && total < job->last_progress_total) {
        clear_progress_line(job);
    }
    if (has_total && job->last_progress_total_num_width > 0
        && total_num_width < job->last_progress_total_num_width) {
        clear_progress_line(job);
    }
    if (job->last_progress_had_total != has_total) {
        clear_progress_line(job);
    }

    char percent_str[DL_PROGRESS_PERCENT_WIDTH + 2];
    char done_str[32];
    char total_str[32];
    char speed_str[32];
    char suffix[192];
    char line[DL_PROGRESS_LINE_BUF_SIZE];

    format_progress_percent(percent, percent_str, sizeof percent_str);
    format_data_size_padded(done_bytes, size_unit, done_num_width, done_str, sizeof done_str);
    format_transfer_rate_padded(job->last_speed_bps, speed_unit, DL_PROGRESS_SPEED_NUMBER_WIDTH,
                                speed_str, sizeof speed_str);

    if (has_total) {
        format_data_size_padded(total, size_unit, total_num_width, total_str, sizeof total_str);
        snprintf(suffix, sizeof suffix, " %s, %s, (%s / %s)", percent_str, speed_str, done_str,
                 total_str);
    } else {
        snprintf(suffix, sizeof suffix, " %s, (%s)", speed_str, done_str);
    }

    const int term_cols = avar_terminal_columns();
    int bar_width = avar_progress_bar_width((int) strlen(suffix));
    if (term_cols > 0) {
        bar_width = avar_progress_bar_width_for_columns(term_cols, (int) strlen(suffix));
    }
    if (job->last_progress_bar_width > 0 && bar_width != job->last_progress_bar_width) {
        clear_progress_line(job);
    }

    AvarProgressStyle progress_style = AVAR_PROGRESS_SEGMENTED;
    char *style_cfg =
            get_config_or_default(AVAR_CFG_DM_PROGRESS_STYLE, AVAR_DEFAULT_PROGRESS_STYLE);
    (void) avar_progress_style_parse(style_cfg, &progress_style);
    free(style_cfg);

    char bar[DL_PROGRESS_BAR_WIDTH_MAX + 3];
    const bool use_segmented = has_total && job->segment_mode && job->seg_cfg.concurrency > 1U
                               && progress_style == AVAR_PROGRESS_SEGMENTED;
    if (use_segmented) {
        format_spatial_progress_bar_fn(total, progress_column_filled, job, bar_width, bar, sizeof bar);
        const int filled_cols = progress_bar_equals_count(bar);
        const int expected_cols = (percent * bar_width + 99) / 100;
        if (filled_cols > expected_cols + 1) {
            format_progress_bar(percent, bar_width, bar, sizeof bar);
        }
    } else {
        format_progress_bar(percent, bar_width, bar, sizeof bar);
    }

    snprintf(line, sizeof line, "%s%s", bar, suffix);

    if (term_cols > 0) {
        while (strlen(line) > (size_t) term_cols && bar_width > DL_PROGRESS_BAR_WIDTH_MIN) {
            bar_width--;
            if (use_segmented) {
                format_spatial_progress_bar_fn(total, progress_column_filled, job, bar_width, bar,
                                               sizeof bar);
                const int filled_cols = progress_bar_equals_count(bar);
                const int expected_cols = (percent * bar_width + 99) / 100;
                if (filled_cols > expected_cols + 1) {
                    format_progress_bar(percent, bar_width, bar, sizeof bar);
                }
            } else {
                format_progress_bar(percent, bar_width, bar, sizeof bar);
            }
            snprintf(line, sizeof line, "%s%s", bar, suffix);
        }
    }

    write_progress_line(job, line);
    job->last_progress_total = total;
    job->last_progress_total_num_width = total_num_width;
    job->last_progress_had_total = has_total;
    job->last_progress_bar_width = bar_width;
    job->last_progress_ms = now;
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

static ChunkSlot *chunk_slot_find(DownloadJob *job, const struct mg_connection *c) {
    if (job == NULL || job->slots == NULL || c == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (job->slots[i].in_use && job->slots[i].conn == c) {
            return &job->slots[i];
        }
    }

    return NULL;
}

static DlConnCtx *dl_conn_ctx_create(DownloadJob *job, ChunkSlot *slot) {
    DlConnCtx *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->job = job;
    ctx->slot = slot;
    return ctx;
}

static void dl_conn_ctx_free(struct mg_connection *c) {
    if (c == NULL || c->fn_data == NULL) {
        return;
    }

    free(c->fn_data);
    c->fn_data = NULL;
}

static bool dl_connect(DownloadJob *job, ChunkSlot *slot, struct mg_connection **conn_out) {
    if (job == NULL || conn_out == NULL) {
        return false;
    }

    DlConnCtx *ctx = dl_conn_ctx_create(job, slot);
    if (ctx == NULL) {
        set_error(job, "Out of memory");
        return false;
    }

    struct mg_connection *c = mg_http_connect(&job->mgr, job->current_url, dl_handler, ctx);
    if (c == NULL) {
        free(ctx);
        set_error(job, "Failed to connect to %s", job->current_url);
        return false;
    }

    if (slot != NULL) {
        slot->conn = c;
    }

    *conn_out = c;
    return true;
}

static ChunkSlot *chunk_slot_acquire(DownloadJob *job, const size_t chunk_index) {
    if (job == NULL || job->slots == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (!job->slots[i].in_use) {
            ChunkSlot *slot = &job->slots[i];
            memset(slot, 0, sizeof(*slot));
            slot->in_use = true;
            slot->chunk_index = chunk_index;
            return slot;
        }
    }

    return NULL;
}

static void chunk_slot_release(DownloadJob *job, ChunkSlot *slot) {
    if (job == NULL || slot == NULL) {
        return;
    }

    memset(slot, 0, sizeof(*slot));
}

static size_t count_active_slots(const DownloadJob *job) {
    if (job == NULL || job->slots == NULL) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (job->slots[i].in_use) {
            count++;
        }
    }
    return count;
}

static bool chunk_in_flight_predicate(const void *ctx, const size_t chunk_index) {
    const DownloadJob *job = (const DownloadJob *)ctx;
    if (job == NULL || job->slots == NULL) {
        return true;
    }

    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (job->slots[i].in_use && job->slots[i].chunk_index == chunk_index) {
            return false;
        }
    }

    return true;
}

static void disable_segment_mode(DownloadJob *job) {
    if (job == NULL) {
        return;
    }

    job->segment_mode = false;
    job->step = DL_STEP_STREAM;
    job->use_ranges = false;

    if (job->slots != NULL) {
        for (size_t i = 0; i < job->slot_capacity; i++) {
            if (job->slots[i].in_use && job->slots[i].conn != NULL) {
                job->slots[i].conn->is_draining = 1;
            }
        }
    }
}

static bool try_enable_segment_mode(DownloadJob *job) {
    if (job == NULL || job->state == NULL || !job->use_ranges) {
        return false;
    }

    if (job->stream_received > 0) {
        return false;
    }

    if (!segment_should_enable(&job->seg_cfg, job->state->total_size, job->use_ranges)) {
        return false;
    }

    if (job->state->chunk_count == 0
        && download_state_init_chunks(job->state, job->state->total_size, job->seg_cfg.chunk_size)
               != 0) {
        return false;
    }

    job->segment_mode = true;
    job->step = DL_STEP_CHUNK;
    return true;
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

static void send_request_for_slot(DownloadJob *job, struct mg_connection *c, ChunkSlot *slot) {
    if (c->data[0] != '\0') {
        return;
    }

    c->data[0] = 1;
    if (slot != NULL) {
        slot->request_sent = true;
    } else {
        job->request_sent = true;
    }

    if (slot != NULL && job->step == DL_STEP_CHUNK) {
        uint64_t start = 0;
        uint64_t end = 0;
        segment_chunk_range(job->state, slot->chunk_index, &start, &end);
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
    if (job->mutex != NULL) {
        avar_mutex_lock(job->mutex);
    }

    if (job->fp == NULL) {
        if (!open_temp_file(job, "r+b") && !open_temp_file(job, "w+b")) {
            if (job->mutex != NULL) {
                avar_mutex_unlock(job->mutex);
            }
            set_error(job, "Failed to open temp file: %s", job->temp_path);
            return false;
        }
    }

#if defined(_WIN32)
    if (_fseeki64(job->fp, (__int64)offset, SEEK_SET) != 0) {
#else
    if (fseeko(job->fp, (off_t)offset, SEEK_SET) != 0) {
#endif
        if (job->mutex != NULL) {
            avar_mutex_unlock(job->mutex);
        }
        set_error(job, "Failed to seek in temp file");
        return false;
    }

    if (!fwrite_chunks(job->fp, data, len)) {
        if (job->mutex != NULL) {
            avar_mutex_unlock(job->mutex);
        }
        set_error(job, "Failed to write temp file: %s", strerror(errno));
        return false;
    }

    fflush(job->fp);
    job->last_activity_ms = mg_millis();

    if (job->mutex != NULL) {
        avar_mutex_unlock(job->mutex);
    }
    return true;
}

static void sync_temp_file(DownloadJob *job) {
    if (job == NULL || job->fp == NULL) {
        return;
    }

    fflush(job->fp);
#if defined(_WIN32)
    _commit(_fileno(job->fp));
#else
    fsync(fileno(job->fp));
#endif
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
    (void) dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
}

static void begin_chunk_body(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c,
                             struct mg_http_message *hm) {
    if (job == NULL) {
        return;
    }

    const size_t chunk_index = slot != NULL ? slot->chunk_index : job->active_chunk;
    if (slot != NULL) {
        slot->streaming = true;
    } else {
        job->streaming = true;
    }
    c->pfn = NULL;

    uint64_t end = 0;
    uint64_t write_offset = 0;
    segment_chunk_range(job->state, chunk_index, &write_offset, &end);
    if (slot != NULL) {
        slot->chunk_write_offset = write_offset;
        slot->chunk_received = 0;
    } else {
        job->chunk_write_offset = write_offset;
        job->chunk_received = 0;
    }

    const size_t to_write = buffered_http_body(c, hm);
    if (to_write > 0) {
        if (!write_at_offset(job, hm->body.buf, to_write, write_offset)) {
            c->is_draining = 1;
            return;
        }
        if (slot != NULL) {
            slot->chunk_received += to_write;
        } else {
            job->chunk_received += to_write;
        }
    }

    mg_iobuf_del(&c->recv, 0, hm->head.len + to_write);
    print_progress(job);
    (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
}

static bool complete_chunk_slot(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c) {
    if (job == NULL || slot == NULL || job->state == NULL) {
        return false;
    }

    if (job->state->chunks_done[slot->chunk_index]) {
        chunk_slot_release(job, slot);
        return true;
    }

    flush_recv_body(job, slot, c);

    if (slot->chunk_expected > 0 && slot->chunk_received < slot->chunk_expected) {
        set_error(job, "Incomplete chunk %zu (%llu / %llu bytes)", slot->chunk_index,
                  (unsigned long long)slot->chunk_received,
                  (unsigned long long)slot->chunk_expected);
        return false;
    }

    sync_temp_file(job);

    if (job->mutex != NULL) {
        avar_mutex_lock(job->mutex);
    }
    job->state->chunks_done[slot->chunk_index] = true;
    if (job->mutex != NULL) {
        avar_mutex_unlock(job->mutex);
    }

    slot->streaming = false;
    slot->chunk_received = 0;
    slot->chunk_expected = 0;
    chunk_slot_release(job, slot);
    (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
    print_progress(job);
    return true;
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

    (void) dm_item_upsert(job, AVAR_DL_STATUS_COMPLETED);
    remove_job_work_dir(job->state_path);
    print_progress(job);
    job->done = true;
    return true;
}

static void request_close(DownloadJob *job, struct mg_connection *c, const bool schedule_after_close) {
    job->pending_schedule = schedule_after_close;
    c->is_draining = 1;
}

static void flush_recv_body(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c) {
    if (job == NULL || c == NULL || c->recv.len == 0) {
        return;
    }

    if (slot != NULL && slot->streaming) {
        const size_t len = c->recv.len;
        if (!write_at_offset(job, c->recv.buf, len, slot->chunk_write_offset + slot->chunk_received)) {
            return;
        }
        slot->chunk_received += len;
        c->recv.len = 0;
        return;
    }

    if (!job->streaming || c->recv.len == 0) {
        return;
    }

    if (job->step == DL_STEP_CHUNK) {
        const size_t len = c->recv.len;
        if (!write_at_offset(job, c->recv.buf, len, job->chunk_write_offset + job->chunk_received)) {
            return;
        }
        job->chunk_received += len;
    } else if (!append_stream(job, c->recv.buf, c->recv.len)) {
        return;
    }

    c->recv.len = 0;
}

static bool try_parse_pending_response(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c) {
    if ((slot != NULL ? slot->headers_received : job->headers_received) || c == NULL
        || c->recv.len == 0) {
        return false;
    }

    struct mg_http_message hm;
    const int n = mg_http_parse((char *)c->recv.buf, c->recv.len, &hm);
    if (n <= 0) {
        return false;
    }

    if (slot != NULL) {
        slot->headers_received = true;
    } else {
        job->headers_received = true;
    }
    handle_response_headers(job, slot, c, &hm);
    return true;
}

static void on_connection_closed(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c) {
    if (job == NULL || job->done || job->failed) {
        dl_conn_ctx_free(c);
        return;
    }

    const bool headers_received = slot != NULL ? slot->headers_received : job->headers_received;
    const bool streaming = slot != NULL ? slot->streaming : job->streaming;
    const bool request_sent = slot != NULL ? slot->request_sent : job->request_sent;

    LOG_DEBUG("Connection closed, step=%d streaming=%d pending_schedule=%d headers_received=%d",
              (int)job->step, streaming, job->pending_schedule, headers_received);

    if (c != NULL && !headers_received) {
        (void)try_parse_pending_response(job, slot, c);
    }

    flush_recv_body(job, slot, c);

    if (job->pending_schedule) {
        job->pending_schedule = false;
        if (slot != NULL) {
            chunk_slot_release(job, slot);
        }
        job->schedule_deferred = true;
        dl_conn_ctx_free(c);
        return;
    }

    if (job->step == DL_STEP_CHUNK && headers_received
        && (streaming || (slot != NULL ? slot->chunk_received : job->chunk_received) > 0)) {
        if (slot != NULL) {
            if (!complete_chunk_slot(job, slot, c)) {
                chunk_slot_release(job, slot);
                job->schedule_deferred = true;
                dl_conn_ctx_free(c);
                return;
            }
        } else {
            flush_recv_body(job, NULL, c);
            if (job->chunk_expected > 0 && job->chunk_received < job->chunk_expected) {
                set_error(job, "Incomplete chunk %zu (%llu / %llu bytes)", job->active_chunk,
                          (unsigned long long)job->chunk_received,
                          (unsigned long long)job->chunk_expected);
                (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
                dl_conn_ctx_free(c);
                return;
            }
            sync_temp_file(job);
            if (job->mutex != NULL) {
                avar_mutex_lock(job->mutex);
            }
            job->state->chunks_done[job->active_chunk] = true;
            if (job->mutex != NULL) {
                avar_mutex_unlock(job->mutex);
            }
            job->streaming = false;
            job->chunk_received = 0;
            job->chunk_expected = 0;
            (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
            print_progress(job);
        }

        if (download_state_all_chunks_done(job->state)) {
            (void)finalize_download(job);
        } else {
            job->schedule_deferred = true;
        }
        dl_conn_ctx_free(c);
        return;
    }

    if (job->step == DL_STEP_STREAM && (streaming || job->stream_received > 0)) {
        if (job->stream_expected == 0 || job->stream_received >= job->stream_expected) {
            (void)finalize_download(job);
        } else {
            set_error(job, "Connection closed before download completed (%llu / %llu bytes)",
                      (unsigned long long)job->stream_received,
                      (unsigned long long)job->stream_expected);
            (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
        }
        dl_conn_ctx_free(c);
        return;
    }

    if (job->step == DL_STEP_CHUNK) {
        if (slot != NULL) {
            chunk_slot_release(job, slot);
        }
        if (download_state_all_chunks_done(job->state)) {
            (void)finalize_download(job);
        } else if (count_active_slots(job) == 0) {
            job->schedule_deferred = true;
        } else {
            job->schedule_deferred = true;
        }
        dl_conn_ctx_free(c);
        return;
    }

    if (!request_sent) {
        if (job->redirect_count > 0) {
            set_error(job, "Connection failed while following redirect to %s",
                      job->current_url != NULL ? job->current_url : job->url);
        } else {
            set_error(job, "Failed to send download request");
        }
    } else {
        set_error(job, "Connection closed before the download started");
    }
    (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
    dl_conn_ctx_free(c);
}

static void schedule_pending_chunks(DownloadJob *job) {
    if (job == NULL || job->failed || job->done || job->step != DL_STEP_CHUNK
        || job->state == NULL) {
        return;
    }

    if (download_state_all_chunks_done(job->state)) {
        (void)finalize_download(job);
        return;
    }

    const size_t active = count_active_slots(job);
    if (active >= job->seg_cfg.concurrency) {
        return;
    }

    const size_t capacity = job->seg_cfg.concurrency - active;
    size_t indices[32];
    const size_t max_select = capacity < (sizeof indices / sizeof indices[0]) ? capacity
                                                                              : (sizeof indices / sizeof indices[0]);
    const size_t selected =
            segment_select_chunks(&job->seg_cfg, job->state, max_select, chunk_in_flight_predicate,
                                  job, indices);
    if (selected == 0) {
        return;
    }

    for (size_t i = 0; i < selected; i++) {
        ChunkSlot *slot = chunk_slot_acquire(job, indices[i]);
        if (slot == NULL) {
            break;
        }

        struct mg_connection *c = NULL;
        if (!dl_connect(job, slot, &c)) {
            chunk_slot_release(job, slot);
            return;
        }
    }
}

static void schedule_next(DownloadJob *job) {
    LOG_DEBUG("Scheduling a job, jobId: %s", job->item_id);

    if (job->failed || job->done) {
        return;
    }

    if (job->step == DL_STEP_CHUNK && job->segment_mode) {
        schedule_pending_chunks(job);
        return;
    }

    job->headers_received = false;
    job->request_sent = false;
    job->streaming = false;
    job->chunk_received = 0;
    job->chunk_expected = 0;
    job->chunk_write_offset = 0;

    if (job->step == DL_STEP_CHUNK) {
        const size_t chunk = next_chunk_index(job);
        if (chunk >= job->state->chunk_count) {
            (void)finalize_download(job);
            return;
        }
        job->active_chunk = chunk;
    }

    struct mg_connection *c = NULL;
    if (!dl_connect(job, NULL, &c)) {
        return;
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

static void handle_response_headers(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c,
                                    struct mg_http_message *hm) {
    if (slot != NULL) {
        slot->headers_received = true;
    } else {
        job->headers_received = true;
    }

    const int status = mg_http_status(hm);

    if (handle_redirect(job, c, hm)) {
        return;
    }

    if (status == 416) {
        if (job->step == DL_STEP_CHUNK && download_state_all_chunks_done(job->state)) {
            (void)finalize_download(job);
        } else {
            set_error(job, "Server rejected range request");
        }
        request_close(job, c, false);
        return;
    }

    if (job->step == DL_STEP_CHUNK && status != 206) {
        LOG_DEBUG("Range request returned HTTP %d; falling back to stream mode", status);
        disable_segment_mode(job);
        if (slot != NULL) {
            chunk_slot_release(job, slot);
        }
        job->schedule_deferred = true;
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
        (void)store_header_value(&job->state->etag, *etag);
    }

    struct mg_str *last_modified = mg_http_get_header(hm, "Last-Modified");
    if (last_modified != NULL) {
        (void)store_header_value(&job->state->last_modified, *last_modified);
    }

    struct mg_str *accept_ranges = mg_http_get_header(hm, "Accept-Ranges");
    if (accept_ranges != NULL) {
        job->use_ranges = mg_strcasecmp(*accept_ranges, mg_str("none")) != 0;
    }

    struct mg_str *content_range = mg_http_get_header(hm, "Content-Range");
    if (content_range != NULL && job->state->total_size == 0) {
        (void)parse_content_range_total(*content_range, &job->state->total_size);
    }

    struct mg_str *content_length = mg_http_get_header(hm, "Content-Length");
    if (content_length != NULL && content_length->len < 32) {
        char buffer[32];
        memcpy(buffer, content_length->buf, content_length->len);
        buffer[content_length->len] = '\0';
        const uint64_t len = strtoull(buffer, NULL, 10);

        if (slot != NULL && job->step == DL_STEP_CHUNK) {
            slot->chunk_expected = len;
        } else if (job->step == DL_STEP_CHUNK) {
            job->chunk_expected = len;
        } else if (job->step == DL_STEP_STREAM && job->stream_expected == 0) {
            job->stream_expected = job->stream_received + len;
            if (job->state->total_size == 0) {
                job->state->total_size = job->stream_expected;
            }
        }
    }

    if (job->step == DL_STEP_STREAM && job->state->total_size > 0 && !job->segment_mode
        && try_enable_segment_mode(job)) {
        clear_progress_line(job);
        request_close(job, c, false);
        job->schedule_deferred = true;
        return;
    }

    if (slot != NULL && job->step == DL_STEP_CHUNK && slot->chunk_expected == 0) {
        uint64_t start = 0;
        uint64_t end = 0;
        segment_chunk_range(job->state, slot->chunk_index, &start, &end);
        slot->chunk_expected = end - start + 1U;
    } else if (slot == NULL && job->step == DL_STEP_CHUNK && job->chunk_expected == 0) {
        uint64_t start = 0;
        uint64_t end = 0;
        segment_chunk_range(job->state, job->active_chunk, &start, &end);
        job->chunk_expected = end - start + 1U;
    }

    if (job->step == DL_STEP_CHUNK) {
        if (slot != NULL) {
            begin_chunk_body(job, slot, c, hm);
        } else {
            begin_chunk_body(job, NULL, c, hm);
        }
    } else if (job->step == DL_STEP_STREAM) {
        begin_stream_body(job, c, hm);
    }
}

static void dl_handler(struct mg_connection *c, int ev, void *ev_data) {
    DlConnCtx *ctx = (DlConnCtx *)c->fn_data;
    if (ctx == NULL) {
        return;
    }

    DownloadJob *job = ctx->job;
    ChunkSlot *slot = ctx->slot;
    if (job == NULL || job->done) {
        return;
    }

    if (ev == MG_EV_POLL) {
        if (mg_millis() > job->connect_deadline_ms && (c->is_connecting || c->is_resolving)) {
            mg_error(c, "Connect timeout");
            LOG_ERROR("Connect timeout, jobId: %s", job->item_id);
        }

        if (!job->failed && mg_millis() - job->last_activity_ms > DL_IDLE_TIMEOUT_MS) {
            mg_error(c, "Download stalled");
            LOG_ERROR("Download stalled, jobId: %s", job->item_id);
        }
    }

    LOG_DUMP("Mongoose event '%s' (%d) was received", mg_event_message(ev), ev);

    if (ev == MG_EV_OPEN) {
        job->connect_deadline_ms = mg_millis() + DL_CONNECT_TIMEOUT_MS;
        job->last_activity_ms = mg_millis();
    } else if (ev == MG_EV_CONNECT) {
        c->data[0] = '\0';
        if (slot != NULL) {
            slot->headers_received = false;
            slot->request_sent = false;
            slot->streaming = false;
        } else {
            job->headers_received = false;
            job->request_sent = false;
            job->streaming = false;
        }
        if (c->is_tls) {
            const struct mg_str host = mg_url_host(job->current_url);
            LOG_DEBUG("Starting TLS to %.*s", (int)host.len, host.buf != NULL ? host.buf : "");
            init_download_tls(c, job->current_url);
        } else {
            send_request_for_slot(job, c, slot);
        }
    } else if (ev == MG_EV_TLS_HS) {
        send_request_for_slot(job, c, slot);
    } else if (ev == MG_EV_WRITE) {
        const bool request_sent = slot != NULL ? slot->request_sent : job->request_sent;
        if (!request_sent && !c->is_connecting && !c->is_tls_hs) {
            send_request_for_slot(job, c, slot);
        }
    } else if (ev == MG_EV_HTTP_HDRS) {
        handle_response_headers(job, slot, c, (struct mg_http_message *)ev_data);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        const bool streaming = slot != NULL ? slot->streaming : job->streaming;
        const bool headers_received = slot != NULL ? slot->headers_received : job->headers_received;
        const size_t chunk_index = slot != NULL ? slot->chunk_index : job->active_chunk;

        if (streaming || headers_received) {
            return;
        }

        if (job->step != DL_STEP_CHUNK) {
            return;
        }

        if (hm->body.len == 0) {
            return;
        }

        uint64_t offset = 0;
        uint64_t end = 0;
        segment_chunk_range(job->state, chunk_index, &offset, &end);
        if (!write_at_offset(job, hm->body.buf, hm->body.len, offset)) {
            request_close(job, c, false);
            return;
        }

        sync_temp_file(job);
        if (job->mutex != NULL) {
            avar_mutex_lock(job->mutex);
        }
        job->state->chunks_done[chunk_index] = true;
        if (job->mutex != NULL) {
            avar_mutex_unlock(job->mutex);
        }
        if (slot != NULL) {
            chunk_slot_release(job, slot);
        }
        (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
        print_progress(job);

        if (download_state_all_chunks_done(job->state)) {
            request_close(job, c, false);
            (void)finalize_download(job);
        } else {
            request_close(job, c, true);
        }
    } else if (ev == MG_EV_READ) {
        const bool streaming = slot != NULL ? slot->streaming : job->streaming;
        if (streaming && c->recv.len > 0) {
            if (slot != NULL) {
                const size_t len = c->recv.len;
                if (!write_at_offset(job, c->recv.buf, len,
                                     slot->chunk_write_offset + slot->chunk_received)) {
                    request_close(job, c, false);
                    return;
                }
                slot->chunk_received += len;
            } else if (job->step == DL_STEP_CHUNK) {
                const size_t len = c->recv.len;
                if (!write_at_offset(job, c->recv.buf, len,
                                     job->chunk_write_offset + job->chunk_received)) {
                    request_close(job, c, false);
                    return;
                }
                job->chunk_received += len;
            } else {
                const size_t len = c->recv.len;
                if (!append_stream(job, c->recv.buf, len)) {
                    request_close(job, c, false);
                    return;
                }
            }
            c->recv.len = 0;

            if (mg_millis() - job->last_progress_ms >= 200) {
                print_progress(job);
                (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
            }
        }
    } else if (ev == MG_EV_CLOSE) {
        on_connection_closed(job, slot, c);
    } else if (ev == MG_EV_ERROR) {
        if (!job->done && !job->failed) {
            const char *message = (const char *)ev_data;
            LOG_DEBUG("Connection error on %s: %s", job->current_url != NULL ? job->current_url : job->url,
                      message != NULL && message[0] != '\0' ? message : "(none)");
            if (slot != NULL && job->segment_mode) {
                chunk_slot_release(job, slot);
                job->schedule_deferred = true;
            } else {
                if (message != NULL && message[0] != '\0') {
                    set_error(job, "%s", message);
                } else {
                    set_error(job, "Network error");
                }
                (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
            }
        }
    }
}

static int run_download(DownloadJob *job) {
    mg_mgr_init(&job->mgr);
    mg_log_set(MG_LL_ERROR);

    job->seg_cfg = download_config_load(job->state != NULL ? job->state->queue_id : NULL);
    job->mutex = avar_mutex_create();
    job->slot_capacity = job->seg_cfg.concurrency;
    if (job->slot_capacity > 0) {
        job->slots = calloc(job->slot_capacity, sizeof(ChunkSlot));
        if (job->slots == NULL) {
            set_error(job, "Out of memory");
            return EXIT_FAILURE;
        }
    }

    job->stream_received = 0;
    job->last_activity_ms = mg_millis();
    job->last_progress_ms = mg_millis();
    job->last_speed_sample_ms = mg_millis();

    if (job->state->total_size > 0 && job->state->chunk_count > 0
        && !download_state_all_chunks_done(job->state)) {
        job->step = DL_STEP_CHUNK;
        if (segment_should_enable(&job->seg_cfg, job->state->total_size, job->use_ranges)) {
            job->segment_mode = true;
        }
        job->active_chunk = next_chunk_index(job);
    } else {
        job->step = DL_STEP_STREAM;
    }

    if (job->step == DL_STEP_STREAM) {
        job->stream_received = existing_file_size(job->temp_path);
        if (job->stream_received > 0 && job->state->total_size == 0
            && download_state_bytes_done(job->state) == 0) {
            remove(job->temp_path);
            job->stream_received = 0;
        }
    }

    job->last_speed_bytes = job_bytes_done(job);

    job->current_url = strdup(job->url);
    if (job->current_url == NULL) {
        set_error(job, "Out of memory");
        return EXIT_FAILURE;
    }

    LOG_DEBUG("Scheduling download job, jobId: %s segment_mode=%d concurrency=%zu", job->item_id,
              job->segment_mode, job->seg_cfg.concurrency);
    schedule_next(job);
    while (!job->done && !job->failed) {
        mg_mgr_poll(&job->mgr, (int)DL_POLL_MS);
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
    free(job->slots);
    avar_mutex_destroy(job->mutex);
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

static int run_transient_download(const char *url, const char *queue, const char *name) {
    (void)name;

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
        char generated[AVAR_DL_ID_BUF_SIZE];
        snprintf(generated, sizeof generated, "%s%llu", AVAR_DL_ID_PREFIX,
                 (unsigned long long) time(NULL));
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
            state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
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
            state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
            if (queue != NULL) {
                char *resolved = queue_resolve_id(queue, true);
                if (resolved == NULL) {
                    resolved = queue_resolve_id(queue, false);
                }
                free(state->queue_id);
                state->queue_id = resolved != NULL ? resolved : strdup(queue);
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
    job->last_speed_sample_ms = mg_millis();

    LOG_INFO("Downloading %s <- %s", dest_path, url);
    (void) dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);

    const int rc = run_download(job);
    if (rc != EXIT_SUCCESS && !job->failed) {
        (void) dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
    }
    job_free(job);
    return rc;
}

int transient_download(const char *url, const char *queue, const char *name, const bool attached) {
    if (!attached) {
        LOG_ERROR("Only --attached downloads are supported in local CLI mode");
        return EXIT_FAILURE;
    }
    return run_transient_download(url, queue, name);
}

#if defined(_WIN32)
static unsigned __stdcall background_download_thread(void *arg) {
#else
static void *background_download_thread(void *arg) {
#endif
    BackgroundDownloadArgs *args = (BackgroundDownloadArgs *)arg;
    if (args == NULL) {
#if defined(_WIN32)
        return 0;
#else
        return NULL;
#endif
    }

    atomic_fetch_add(&g_active_downloads, 1U);
    (void)run_transient_download(args->url, args->queue, args->name);
    atomic_fetch_sub(&g_active_downloads, 1U);

    free(args->url);
    free(args->queue);
    free(args->name);
    free(args);

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static void background_download_task(void *arg) {
    (void)background_download_thread(arg);
}

int download_start_background(const char *url, const char *queue, const char *name, char **id_out) {
    if (url == NULL || !is_valid_http_url(url)) {
        return EXIT_FAILURE;
    }

    BackgroundDownloadArgs *args = calloc(1, sizeof(*args));
    if (args == NULL) {
        return EXIT_FAILURE;
    }

    args->url = strdup(url);
    args->queue = queue != NULL ? strdup(queue) : NULL;
    args->name = name != NULL ? strdup(name) : NULL;
    if (args->url == NULL || (queue != NULL && args->queue == NULL) ||
        (name != NULL && args->name == NULL)) {
        free(args->url);
        free(args->queue);
        free(args->name);
        free(args);
        return EXIT_FAILURE;
    }

    if (id_out != NULL) {
        char generated[AVAR_DL_ID_BUF_SIZE];
        snprintf(generated, sizeof generated, "%s%llu", AVAR_DL_ID_PREFIX,
                 (unsigned long long)time(NULL));
        *id_out = strdup(generated);
    }

    ThreadPool *pool = thread_pool_global();
    if (pool != NULL && thread_pool_submit(pool, background_download_task, args)) {
        return EXIT_SUCCESS;
    }

#if defined(_WIN32)
    const uintptr_t handle = _beginthreadex(NULL, 0, background_download_thread, args, 0, NULL);
    if (handle == 0U) {
        free(args->url);
        free(args->queue);
        free(args->name);
        free(args);
        return EXIT_FAILURE;
    }
    CloseHandle((HANDLE)handle);
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, background_download_thread, args) != 0) {
        free(args->url);
        free(args->queue);
        free(args->name);
        free(args);
        return EXIT_FAILURE;
    }
    pthread_detach(thread);
#endif

    return EXIT_SUCCESS;
}
