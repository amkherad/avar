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
#include <http_proxy.h>
#include <queue.h>
#include <http.h>
#include <stream_hls.h>
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
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/* -------------------------------------------------------------------------- */
/* Platform abstractions                                                      */
/*                                                                            */
/* These thin wrappers keep the rest of the file free of #if branches so the  */
/* download logic reads the same on every platform.                           */
/* -------------------------------------------------------------------------- */

static void platform_sleep_ms(const unsigned ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000U);
#endif
}

static void platform_gmtime(const time_t *t, struct tm *out) {
#if defined(_WIN32)
    gmtime_s(out, t);
#else
    gmtime_r(t, out);
#endif
}

static int platform_seek(FILE *fp, const uint64_t offset) {
#if defined(_WIN32)
    return _fseeki64(fp, (__int64)offset, SEEK_SET);
#else
    return fseeko(fp, (off_t)offset, SEEK_SET);
#endif
}

static void platform_fsync(FILE *fp) {
    fflush(fp);
#if defined(_WIN32)
    _commit(_fileno(fp));
#else
    fsync(fileno(fp));
#endif
}

static void platform_remove_dir(const char *dir) {
#if defined(_WIN32)
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}

/* Enables ANSI escape handling on the Windows console once; no-op elsewhere. */
static void platform_enable_ansi_terminal(void) {
#if defined(_WIN32)
    static bool enabled = false;
    if (enabled || !avar_stderr_is_tty()) {
        return;
    }

    const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode)) {
        SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    enabled = true;
#endif
}

static atomic_uint g_active_downloads = 0;

typedef struct {
    char id[AVAR_DL_ID_BUF_SIZE];
    unsigned refcount;
} ProgressWatcher;

static struct {
    AvarMutex *mutex;
    ProgressWatcher *items;
    size_t count;
    size_t capacity;
} g_progress_watchers = {0};

static void progress_watchers_init(void) {
    if (g_progress_watchers.mutex == NULL) {
        g_progress_watchers.mutex = avar_mutex_create();
    }
}

static ssize_t progress_watcher_index(const char *id) {
    if (id == NULL) {
        return -1;
    }

    for (size_t i = 0U; i < g_progress_watchers.count; i++) {
        if (strcmp(g_progress_watchers.items[i].id, id) == 0) {
            return (ssize_t)i;
        }
    }

    return -1;
}

static bool progress_watchers_grow(void) {
    const size_t next_capacity = g_progress_watchers.capacity == 0U ? 8U
                                                                 : g_progress_watchers.capacity * 2U;
    ProgressWatcher *items =
        realloc(g_progress_watchers.items, next_capacity * sizeof(ProgressWatcher));
    if (items == NULL) {
        return false;
    }

    g_progress_watchers.items = items;
    g_progress_watchers.capacity = next_capacity;
    return true;
}

void download_progress_watch(const char *id) {
    if (id == NULL || id[0] == '\0') {
        return;
    }

    progress_watchers_init();
    avar_mutex_lock(g_progress_watchers.mutex);

    const ssize_t index = progress_watcher_index(id);
    if (index >= 0) {
        g_progress_watchers.items[(size_t)index].refcount++;
        avar_mutex_unlock(g_progress_watchers.mutex);
        return;
    }

    if (g_progress_watchers.count >= g_progress_watchers.capacity && !progress_watchers_grow()) {
        avar_mutex_unlock(g_progress_watchers.mutex);
        return;
    }

    ProgressWatcher *entry = &g_progress_watchers.items[g_progress_watchers.count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->id, sizeof entry->id, "%s", id);
    entry->refcount = 1U;

    avar_mutex_unlock(g_progress_watchers.mutex);
}

void download_progress_unwatch(const char *id) {
    if (id == NULL || id[0] == '\0' || g_progress_watchers.mutex == NULL) {
        return;
    }

    avar_mutex_lock(g_progress_watchers.mutex);

    const ssize_t index = progress_watcher_index(id);
    if (index < 0) {
        avar_mutex_unlock(g_progress_watchers.mutex);
        return;
    }

    ProgressWatcher *entry = &g_progress_watchers.items[(size_t)index];
    if (entry->refcount > 1U) {
        entry->refcount--;
        avar_mutex_unlock(g_progress_watchers.mutex);
        return;
    }

    const size_t last = g_progress_watchers.count - 1U;
    if ((size_t)index != last) {
        g_progress_watchers.items[(size_t)index] = g_progress_watchers.items[last];
    }
    g_progress_watchers.count--;

    avar_mutex_unlock(g_progress_watchers.mutex);
}

bool download_progress_is_watched(const char *id) {
    if (id == NULL || id[0] == '\0' || g_progress_watchers.mutex == NULL) {
        return false;
    }

    bool watched = false;
    avar_mutex_lock(g_progress_watchers.mutex);
    watched = progress_watcher_index(id) >= 0;
    avar_mutex_unlock(g_progress_watchers.mutex);
    return watched;
}

#define DL_SNOWFLAKE_EPOCH_MS 1704067200000ULL
#define DL_SNOWFLAKE_MACHINE_BITS 10U
#define DL_SNOWFLAKE_SEQUENCE_BITS 12U
#define DL_SNOWFLAKE_MACHINE_MASK ((1U << DL_SNOWFLAKE_MACHINE_BITS) - 1U)
#define DL_SNOWFLAKE_SEQUENCE_MASK ((1U << DL_SNOWFLAKE_SEQUENCE_BITS) - 1U)
#define DL_SNOWFLAKE_MACHINE_SHIFT DL_SNOWFLAKE_SEQUENCE_BITS
#define DL_SNOWFLAKE_TIMESTAMP_SHIFT (DL_SNOWFLAKE_MACHINE_BITS + DL_SNOWFLAKE_SEQUENCE_BITS)

typedef struct {
    AvarMutex *mutex;
    uint64_t last_ms;
    uint32_t sequence;
    uint32_t machine_id;
} DownloadSnowflakeState;

static DownloadSnowflakeState g_download_snowflake = {0};

typedef struct {
    char *url;
    char *queue;
    char *name;
    char *item_id;
    char *proxy;
} BackgroundDownloadArgs;

static void background_args_free(BackgroundDownloadArgs *args) {
    if (args == NULL) {
        return;
    }
    free(args->url);
    free(args->queue);
    free(args->name);
    free(args->item_id);
    free(args->proxy);
    free(args);
}

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
        platform_sleep_ms(step_ms);
    }
    return download_active_count() == 0U;
}

static int find_download_item_index(const char *target, bool by_id);

static int spawn_download_by_id(const char *item_id, const char *proxy_override);

static int download_status_exit_code(const char *status) {
    if (status == NULL) {
        return EXIT_FAILURE;
    }

    if (strcmp(status, AVAR_DL_STATUS_COMPLETED) == 0) {
        return EXIT_SUCCESS;
    }

    if (strcmp(status, AVAR_DL_STATUS_STOPPED) == 0 || strcmp(status, AVAR_DL_STATUS_PAUSED) == 0) {
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

static int wait_for_download_item(const char *item_id) {
    if (item_id == NULL) {
        return EXIT_FAILURE;
    }

    const unsigned step_ms = DL_POLL_MS;
    for (;;) {
        const int index = find_download_item_index(item_id, true);
        if (index < 0) {
            return EXIT_FAILURE;
        }

        char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STATUS);
        if (status == NULL) {
            return EXIT_FAILURE;
        }

        if (strcmp(status, AVAR_DL_STATUS_DOWNLOADING) != 0) {
            const int rc = download_status_exit_code(status);
            free(status);
            return rc;
        }

        free(status);
        platform_sleep_ms(step_ms);
    }
}

static bool download_pool_submit(ThreadPoolTask task, void *arg) {
    ThreadPool *pool = thread_pool_global();
    return pool != NULL && thread_pool_submit(pool, task, arg);
}

typedef enum {
    DL_STEP_CHUNK,
    DL_STEP_STREAM,
} DlStep;

typedef struct DownloadJob DownloadJob;

typedef struct ChunkSlot {
    struct mg_connection *conn;
    uint64_t range_start;
    uint64_t range_end;
    uint64_t chunk_write_offset;
    uint64_t chunk_received;
    uint64_t chunk_expected;
    uint64_t last_activity_ms;
    bool headers_received;
    bool streaming;
    bool request_sent;
    bool in_use;
} ChunkSlot;

typedef struct DlConnCtx {
    DownloadJob *job;
    ChunkSlot *slot;
    char *proxy_url;
    ProxyKind proxy_kind;
    ProxySocksCtx socks;
    bool use_proxy;
    bool proxy_tunnel_done;
    bool proxy_http_forward;
} DlConnCtx;

typedef struct DownloadJob {
    struct mg_mgr mgr;
    char *url;
    char *current_url;
    char *item_id;
    char *filename;
    bool filename_inferred;
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
    bool segment_disabled;
    bool pending_segment_enable;
    DlStep step;
    uint64_t active_range_start;
    uint64_t active_range_end;
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
    size_t segment_error_count;
    uint32_t error_count;
    volatile bool cancel_requested;
    volatile bool pause_requested;
} DownloadJob;

#define AVAR_ACTIVE_JOB_CAP 64U

static struct {
    AvarMutex *mutex;
    DownloadJob *jobs[AVAR_ACTIVE_JOB_CAP];
} g_active_jobs = {0};

static void active_jobs_register(DownloadJob *job);
static void active_jobs_unregister(DownloadJob *job);
static DownloadJob *active_jobs_find(const char *item_id);
static int run_download_for_item_id(const char *item_id, const char *proxy_override);
static void apply_proxy_to_state(DownloadState *state, const char *proxy_override);
static void background_download_task_by_id(void *arg);
static int update_download_item_status(const char *item_id, const char *status);
static void log_download_item_status(const char *item_id, const char *filename, const char *status);
static void log_job_status(const DownloadJob *job, const char *status);

static void set_error(DownloadJob *job, const char *fmt, ...);

static void close_file(DownloadJob *job);

static void sync_temp_file(DownloadJob *job);

static char *build_job_dir(const char *temp_dir, const char *item_id);

static char *build_state_path(const char *job_dir);

static char *format_datetime_iso(void);

static char *find_resumable_item_id(const char *url);

static char *download_generate_id(void);

static uint64_t download_snowflake_next(void);

static uint64_t job_bytes_done(const DownloadJob *job);

static void sync_state_metadata(DownloadJob *job, const char *status);

static void remove_job_work_dir(const char *state_path);

static char *choose_filename(const char *url, const char *header_value, size_t header_len);

static bool read_item_optional_uint32(const char *item_id, const char *field, uint32_t *out);

static uint32_t read_queue_max_retries(const char *queue_id);

static uint32_t resolve_max_retries(const DownloadJob *job);

static bool record_recoverable_error(DownloadJob *job);

static void reset_retry_count_on_success(DownloadJob *job);

static void load_job_retry_state(DownloadJob *job);

static char *build_item_json(const DownloadJob *job, const char *status);

static int dm_item_upsert(DownloadJob *job, const char *status);

static void print_progress(DownloadJob *job);

static void report_cli(const DownloadJob *job);

static bool next_pending_segment(const DownloadJob *job, uint64_t *start_out, uint64_t *end_out);

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

static void touch_slot_activity(ChunkSlot *slot);

static DlConnCtx *dl_conn_ctx_create(DownloadJob *job, ChunkSlot *slot);

static void dl_conn_ctx_destroy(DlConnCtx *ctx);

static void dl_conn_ctx_free(struct mg_connection *c);

static ChunkSlot *chunk_slot_find(DownloadJob *job, const struct mg_connection *c);

static ChunkSlot *chunk_slot_acquire(DownloadJob *job, uint64_t range_start, uint64_t range_end);

static void chunk_slot_release(DownloadJob *job, ChunkSlot *slot);

static size_t count_active_slots(const DownloadJob *job);

static bool range_in_flight_predicate(const void *ctx, uint64_t start, uint64_t end);

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
    platform_gmtime(&now, &tm_utc);
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

    const char *last_sep = strrchr(state_path, PATH_SEPARATOR);
    if (last_sep == NULL) {
        remove(state_path);
        return;
    }

    char *job_dir = strndup(state_path, (size_t)(last_sep - state_path));
    if (job_dir == NULL) {
        remove(state_path);
        return;
    }

    (void)remove_directory_recursive(job_dir);
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

static bool read_item_optional_uint32(const char *item_id, const char *field, uint32_t *out) {
    if (item_id == NULL || field == NULL || out == NULL) {
        return false;
    }

    const int index = find_download_item_index(item_id, true);
    if (index < 0) {
        return false;
    }

    char *value = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, field);
    if (value == NULL || value[0] == '\0') {
        free(value);
        return false;
    }

    char *end = NULL;
    const unsigned long parsed = strtoul(value, &end, 10);
    free(value);
    if (end == value || parsed == 0U) {
        return false;
    }

    *out = (uint32_t)parsed;
    return true;
}

static uint32_t read_queue_max_retries(const char *queue_id) {
    if (queue_id == NULL || queue_id[0] == '\0') {
        return 0U;
    }

    const size_t count = get_config_array_size(AVAR_CFG_DM_QUEUES);
    for (size_t i = 0; i < count; i++) {
        char *id = get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_FIELD_ID);
        if (id == NULL || strcmp(id, queue_id) != 0) {
            free(id);
            continue;
        }
        free(id);

        char *value =
                get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_QUEUE_FIELD_MAX_RETRIES);
        if (value == NULL || value[0] == '\0') {
            free(value);
            return 0U;
        }

        char *end = NULL;
        const unsigned long parsed = strtoul(value, &end, 10);
        free(value);
        if (end == value || parsed == 0U) {
            return 0U;
        }

        return (uint32_t)parsed;
    }

    return 0U;
}

static uint32_t resolve_max_retries(const DownloadJob *job) {
    if (job == NULL || job->item_id == NULL) {
        return DL_DEFAULT_MAX_RETRIES;
    }

    uint32_t item_limit = 0U;
    if (read_item_optional_uint32(job->item_id, AVAR_FIELD_MAX_RETRIES, &item_limit)) {
        return item_limit;
    }

    const char *queue_id = job->state != NULL ? job->state->queue_id : NULL;
    const uint32_t queue_limit = read_queue_max_retries(queue_id);
    if (queue_limit > 0U) {
        return queue_limit;
    }

    return DL_DEFAULT_MAX_RETRIES;
}

static void load_job_retry_state(DownloadJob *job) {
    if (job == NULL || job->item_id == NULL) {
        return;
    }

    uint32_t count = 0U;
    if (read_item_optional_uint32(job->item_id, AVAR_FIELD_ERROR_COUNT, &count)) {
        job->error_count = count;
    }
}

static bool record_recoverable_error(DownloadJob *job) {
    if (job == NULL) {
        return true;
    }

    job->error_count++;
    const uint32_t limit = resolve_max_retries(job);
    if (job->error_count >= limit) {
        set_error(job, "Maximum retry count reached (%u)", limit);
        (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
        return true;
    }

    (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
    return false;
}

static void reset_retry_count_on_success(DownloadJob *job) {
    if (job == NULL || job->error_count == 0U) {
        return;
    }

    job->error_count = 0U;
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
    cJSON_AddBoolToObject(obj, AVAR_FIELD_FILENAME_INFERRED, job->filename_inferred);
    cJSON_AddStringToObject(obj, AVAR_FIELD_STATUS, status);
    if (job->state != NULL && job->state->proxy != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_PROXY, job->state->proxy);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_PROXY);
    }
    cJSON_AddNumberToObject(obj, AVAR_FIELD_BYTES_DOWNLOADED, (double) done_bytes);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_TOTAL_BYTES, (double) total);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_ERROR_COUNT, (double) job->error_count);

    uint32_t max_retries = 0U;
    if (job->item_id != NULL &&
        read_item_optional_uint32(job->item_id, AVAR_FIELD_MAX_RETRIES, &max_retries)) {
        cJSON_AddNumberToObject(obj, AVAR_FIELD_MAX_RETRIES, (double)max_retries);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_MAX_RETRIES);
    }

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
    if (job->state != NULL && job->state->stream_kind != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_STREAM_KIND, job->state->stream_kind);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_STREAM_KIND);
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

    if (job->state != NULL && job->state->total_size > 0U && job->state->chunk_size > 0U
        && download_progress_is_watched(job->item_id)) {
        cJSON_AddNumberToObject(obj, AVAR_FIELD_CHUNK_SIZE, (double)job->state->chunk_size);
        cJSON *ranges = cJSON_AddArrayToObject(obj, "doneRanges");
        if (ranges != NULL) {
            for (size_t i = 0U; i < job->state->done_range_count; i++) {
                cJSON *pair = cJSON_CreateArray();
                if (pair == NULL) {
                    continue;
                }
                cJSON_AddItemToArray(pair,
                                     cJSON_CreateNumber((double)job->state->done_ranges[i].start));
                cJSON_AddItemToArray(pair,
                                     cJSON_CreateNumber((double)job->state->done_ranges[i].end));
                cJSON_AddItemToArray(ranges, pair);
            }
        }
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

    int rc;
    if (update_config_array_item(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, job->item_id, json) != 0) {
        rc = append_config_array_item(AVAR_CFG_DM_ITEMS, json);
    } else {
        rc = 0;
    }

    cJSON_free(json);
    if (rc == 0) {
        log_job_status(job, status);
    }
    return rc;
}

static void clear_progress_line(DownloadJob *job) {
    if (job == NULL || job->progress_line_max_len == 0) {
        return;
    }

    platform_enable_ansi_terminal();

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

    platform_enable_ansi_terminal();

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

static bool column_overlaps_range(const uint64_t col_start, const uint64_t col_end,
                                  const uint64_t range_start, const uint64_t range_end) {
    return col_start < range_end && range_start < col_end;
}

static bool progress_column_filled(const uint64_t col_start, const uint64_t col_end, void *ctx) {
    const DownloadJob *job = (const DownloadJob *) ctx;
    if (job == NULL || col_start >= col_end) {
        return false;
    }

    if (job->state != NULL && job->state->done_range_count > 0U) {
        for (size_t i = 0U; i < job->state->done_range_count; i++) {
            const ByteRange *range = &job->state->done_ranges[i];
            if (column_overlaps_range(col_start, col_end, range->start, range->end + 1U)) {
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

static bool next_pending_segment(const DownloadJob *job, uint64_t *start_out, uint64_t *end_out) {
    if (job == NULL || job->state == NULL || job->state->total_size == 0U || start_out == NULL
        || end_out == NULL) {
        return false;
    }

    const size_t segment_count = download_state_segment_count(job->state);
    for (size_t i = 0U; i < segment_count; i++) {
        if (download_state_is_segment_done(job->state, i)) {
            continue;
        }

        segment_chunk_range(job->state, i, start_out, end_out);
        return true;
    }

    return false;
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
    DlConnCtx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->job = job;
    ctx->slot = slot;
    return ctx;
}

static void dl_conn_ctx_destroy(DlConnCtx *ctx) {
    if (ctx == NULL) {
        return;
    }
    free(ctx->proxy_url);
    free(ctx);
}

static void dl_conn_ctx_free(struct mg_connection *c) {
    if (c == NULL || c->fn_data == NULL) {
        return;
    }

    dl_conn_ctx_destroy((DlConnCtx *)c->fn_data);
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

    const char *proxy_url =
            job->state != NULL && job->state->proxy != NULL ? job->state->proxy : NULL;
    ctx->use_proxy = proxy_url != NULL;
    if (proxy_url != NULL) {
        ctx->proxy_url = strdup(proxy_url);
        if (ctx->proxy_url == NULL) {
            free(ctx);
            set_error(job, "Out of memory");
            return false;
        }
        ctx->proxy_kind = proxy_kind_from_url(proxy_url);
        ctx->proxy_http_forward = proxy_uses_http_forward(proxy_url, job->current_url);
    }

    char *connect_url = proxy_url != NULL ? proxy_connect_url(proxy_url) : NULL;
    if (connect_url == NULL && proxy_url != NULL) {
        dl_conn_ctx_destroy(ctx);
        set_error(job, "Invalid proxy URL");
        return false;
    }

    const char *resolved_connect = connect_url != NULL ? connect_url : job->current_url;
    struct mg_connection *c = mg_http_connect(&job->mgr, resolved_connect, dl_handler, ctx);
    free(connect_url);
    if (c == NULL) {
        dl_conn_ctx_destroy(ctx);
        set_error(job, "Failed to connect to %s", resolved_connect);
        return false;
    }

    if (slot != NULL) {
        slot->conn = c;
    }

    *conn_out = c;
    return true;
}

static void touch_slot_activity(ChunkSlot *slot) {
    if (slot != NULL) {
        slot->last_activity_ms = mg_millis();
    }
}

/* Bytes still expected for this slot's range, or UINT64_MAX when the size is
 * not yet known (no Content-Length parsed). */
static uint64_t slot_bytes_remaining(const ChunkSlot *slot) {
    if (slot->chunk_expected == 0U) {
        return UINT64_MAX;
    }
    return slot->chunk_received >= slot->chunk_expected
                   ? 0U
                   : slot->chunk_expected - slot->chunk_received;
}

/* Writes received body bytes for a segment slot, clamping to the slot's range so
 * a misbehaving server cannot overflow into a neighbouring segment. Any bytes
 * past the range boundary are intentionally dropped. Returns false on I/O error. */
static bool slot_write_body(DownloadJob *job, ChunkSlot *slot, const void *data, size_t len) {
    const uint64_t remaining = slot_bytes_remaining(slot);
    if (remaining != UINT64_MAX && (uint64_t)len > remaining) {
        len = (size_t)remaining;
    }
    if (len == 0U) {
        return true;
    }

    if (!write_at_offset(job, data, len, slot->chunk_write_offset + slot->chunk_received)) {
        return false;
    }

    slot->chunk_received += len;
    touch_slot_activity(slot);
    return true;
}

static ChunkSlot *chunk_slot_acquire(DownloadJob *job, const uint64_t range_start,
                                     const uint64_t range_end) {
    if (job == NULL || job->slots == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (!job->slots[i].in_use) {
            ChunkSlot *slot = &job->slots[i];
            memset(slot, 0, sizeof(*slot));
            slot->in_use = true;
            slot->range_start = range_start;
            slot->range_end = range_end;
            slot->last_activity_ms = mg_millis();
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

static bool range_in_flight_predicate(const void *ctx, const uint64_t start, const uint64_t end) {
    const DownloadJob *job = (const DownloadJob *)ctx;
    if (job == NULL || job->slots == NULL) {
        return true;
    }

    for (size_t i = 0; i < job->slot_capacity; i++) {
        if (job->slots[i].in_use && job->slots[i].range_start == start
            && job->slots[i].range_end == end) {
            return false;
        }
    }

    return true;
}

static void reset_chunk_progress(DownloadState *state) {
    if (state == NULL) {
        return;
    }

    free(state->done_ranges);
    state->done_ranges = NULL;
    state->done_range_count = 0U;
    state->done_range_capacity = 0U;
}

static bool job_has_partial_progress(const DownloadJob *job) {
    if (job == NULL) {
        return false;
    }

    if (job_bytes_done(job) > 0U) {
        return true;
    }

    if (job->temp_path != NULL && existing_file_size(job->temp_path) > 0U) {
        return true;
    }

    return false;
}

static void handle_resume_unsupported(DownloadJob *job) {
    if (job == NULL) {
        return;
    }

    close_file(job);
    job->streaming = false;
    job->done = true;
    job->failed = false;
    job->error[0] = '\0';

    if (job->state != NULL) {
        free(job->state->description);
        job->state->description = strdup(AVAR_DL_DESC_RESUME_UNSUPPORTED);
    }

    (void)dm_item_upsert(job, AVAR_DL_STATUS_STOPPED);
    LOG_INFO("Download %s paused: server does not support resume", job->item_id);
}

static void disable_segment_mode(DownloadJob *job) {
    if (job == NULL) {
        return;
    }

    job->segment_mode = false;
    job->step = DL_STEP_STREAM;
    job->use_ranges = false;
    job->segment_disabled = true;

    close_file(job);

    if (job->slots != NULL) {
        for (size_t i = 0; i < job->slot_capacity; i++) {
            if (job->slots[i].in_use && job->slots[i].conn != NULL) {
                job->slots[i].conn->is_draining = 1;
            }
            memset(&job->slots[i], 0, sizeof(ChunkSlot));
        }
    }

    reset_chunk_progress(job->state);

    if (job->temp_path != NULL) {
        remove(job->temp_path);
    }

    job->stream_received = 0;
    job->stream_expected = 0;
    job->chunk_received = 0;
    job->chunk_expected = 0;
    job->chunk_write_offset = 0;
    job->streaming = false;
    job->headers_received = false;
    job->request_sent = false;
}

static bool can_enable_segment_mode(const DownloadJob *job) {
    if (job == NULL || job->state == NULL || !job->use_ranges || job->segment_disabled) {
        return false;
    }

    if (job->stream_received > 0) {
        return false;
    }

    if (!segment_should_enable(&job->seg_cfg, job->state->total_size, job->use_ranges)) {
        return false;
    }

    return true;
}

static bool apply_segment_mode(DownloadJob *job) {
    if (!can_enable_segment_mode(job)) {
        return false;
    }

    if (download_state_init_chunks(job->state, job->state->total_size, job->seg_cfg.chunk_size)
               != 0) {
        return false;
    }

    job->segment_mode = true;
    job->step = DL_STEP_CHUNK;
    return true;
}

static bool try_enable_segment_mode(DownloadJob *job) {
    return apply_segment_mode(job);
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
    const DlConnCtx *ctx = (const DlConnCtx *)c->fn_data;
    if (ctx != NULL && ctx->proxy_http_forward) {
        uri = job->current_url;
    }

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
        touch_slot_activity(slot);
    } else {
        job->request_sent = true;
    }

    if (slot != NULL && job->step == DL_STEP_CHUNK) {
        send_request(job, c, slot->range_start, slot->range_end);
    } else if (job->step == DL_STEP_CHUNK) {
        send_request(job, c, job->active_range_start, job->active_range_end);
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

    if (platform_seek(job->fp, offset) != 0) {
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

    platform_fsync(job->fp);
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

    const uint64_t range_start = slot != NULL ? slot->range_start : job->active_range_start;
    const uint64_t range_end = slot != NULL ? slot->range_end : job->active_range_end;
    if (slot != NULL) {
        slot->streaming = true;
    } else {
        job->streaming = true;
    }
    c->pfn = NULL;

    const uint64_t write_offset = range_start;
    if (slot != NULL) {
        slot->chunk_write_offset = write_offset;
        slot->chunk_received = 0;
        touch_slot_activity(slot);
    } else {
        job->chunk_write_offset = write_offset;
        job->chunk_received = 0;
    }

    const size_t to_write = buffered_http_body(c, hm);
    if (to_write > 0) {
        const bool ok = slot != NULL
                                ? slot_write_body(job, slot, hm->body.buf, to_write)
                                : write_at_offset(job, hm->body.buf, to_write, write_offset);
        if (!ok) {
            c->is_draining = 1;
            return;
        }
        if (slot == NULL) {
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

    if (download_state_is_range_done(job->state, slot->range_start, slot->range_end)) {
        chunk_slot_release(job, slot);
        return true;
    }

    flush_recv_body(job, slot, c);

    if (slot->chunk_expected > 0 && slot->chunk_received < slot->chunk_expected) {
        /* The connection dropped mid-segment. Leave the range unmarked so the
         * scheduler retries just this segment; the caller decides whether the
         * per-download retry budget has been exhausted. */
        LOG_DEBUG("Incomplete segment %llu-%llu (%llu / %llu bytes), will retry",
                  (unsigned long long)slot->range_start, (unsigned long long)slot->range_end,
                  (unsigned long long)slot->chunk_received,
                  (unsigned long long)slot->chunk_expected);
        return false;
    }

    sync_temp_file(job);

    if (job->mutex != NULL) {
        avar_mutex_lock(job->mutex);
    }
    (void)download_state_mark_range_done(job->state, slot->range_start, slot->range_end);
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
    if (job == NULL || !job->filename_inferred) {
        return true;
    }

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
        job->state->filename_inferred = false;
    }

    job->filename_inferred = false;

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

    char *unique_dest = resolve_unique_dest_path(job->dest_path);
    if (unique_dest == NULL) {
        set_error(job, "Failed to resolve destination path for %s", job->dest_path);
        return false;
    }

    if (strcmp(unique_dest, job->dest_path) != 0) {
        const char *basename = strrchr(unique_dest, PATH_SEPARATOR);
        basename = basename != NULL ? basename + 1 : unique_dest;

        free(job->dest_path);
        job->dest_path = unique_dest;

        free(job->filename);
        job->filename = strdup(basename);
        if (job->filename == NULL) {
            set_error(job, "Failed to update filename for %s", unique_dest);
            return false;
        }

        if (job->state != NULL) {
            free(job->state->dest_path);
            free(job->state->filename);
            job->state->dest_path = strdup(unique_dest);
            job->state->filename = strdup(basename);
            if (job->state->dest_path == NULL || job->state->filename == NULL) {
                set_error(job, "Failed to update state paths for %s", unique_dest);
                return false;
            }
        }
    } else {
        free(unique_dest);
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
        if (!slot_write_body(job, slot, c->recv.buf, c->recv.len)) {
            return;
        }
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

/* Records a recoverable segment failure. Reschedules the pending segments when
 * the retry budget still allows it, otherwise fails the whole download. */
static void note_segment_failure(DownloadJob *job) {
    if (job == NULL || job->state == NULL) {
        return;
    }

    job->segment_error_count++;
    if (record_recoverable_error(job)) {
        return;
    }

    job->schedule_deferred = true;
}

static void on_connection_closed(DownloadJob *job, ChunkSlot *slot, struct mg_connection *c) {
    if (job == NULL || job->done || job->failed) {
        dl_conn_ctx_free(c);
        return;
    }

    /* The slot was already released (e.g. siblings drained when falling back from
     * segmented to streaming mode). Nothing left to do for this connection. */
    if (slot != NULL && (!slot->in_use || slot->conn != c)) {
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
                note_segment_failure(job);
                dl_conn_ctx_free(c);
                return;
            }
        } else {
            flush_recv_body(job, NULL, c);
            if (job->chunk_expected > 0 && job->chunk_received < job->chunk_expected) {
                set_error(job, "Incomplete range %llu-%llu (%llu / %llu bytes)",
                          (unsigned long long)job->active_range_start,
                          (unsigned long long)job->active_range_end,
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
            (void)download_state_mark_range_done(job->state, job->active_range_start,
                                                 job->active_range_end);
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

    if (job->step == DL_STEP_STREAM && job->pending_segment_enable) {
        job->pending_segment_enable = false;
        if (apply_segment_mode(job)) {
            job->schedule_deferred = true;
        } else {
            set_error(job, "Failed to enable segmented download");
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
        } else if (job->segment_mode) {
            /* A segment connection dropped before delivering any data; retry it
             * (subject to the per-download budget) rather than failing. */
            note_segment_failure(job);
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
    ByteRange ranges[32];
    const size_t max_select = capacity < (sizeof ranges / sizeof ranges[0]) ? capacity
                                                                            : (sizeof ranges / sizeof ranges[0]);
    const size_t selected =
            segment_select_chunks(&job->seg_cfg, job->state, max_select, range_in_flight_predicate,
                                  job, ranges);
    if (selected == 0) {
        return;
    }

    for (size_t i = 0; i < selected; i++) {
        ChunkSlot *slot = chunk_slot_acquire(job, ranges[i].start, ranges[i].end);
        if (slot == NULL) {
            break;
        }

        struct mg_connection *c = NULL;
        if (!dl_connect(job, slot, &c)) {
            chunk_slot_release(job, slot);
            continue;
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
        uint64_t start = 0U;
        uint64_t end = 0U;
        if (!next_pending_segment(job, &start, &end)) {
            (void)finalize_download(job);
            return;
        }
        job->active_range_start = start;
        job->active_range_end = end;
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
    /* The slot was released/drained (e.g. another segment already triggered the
     * fallback to streaming). Drain this stale connection instead of letting it
     * drive a second, conflicting download. */
    if (slot != NULL && (!slot->in_use || slot->conn != c)) {
        request_close(job, c, false);
        return;
    }

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
        } else if (job_has_partial_progress(job)) {
            handle_resume_unsupported(job);
        } else {
            set_error(job, "Server rejected range request");
        }
        request_close(job, c, false);
        return;
    }

    if (job->step == DL_STEP_STREAM && job->stream_received > 0U && status == 200) {
        handle_resume_unsupported(job);
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

    reset_retry_count_on_success(job);

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
    if (accept_ranges != NULL && !job->segment_disabled) {
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
        && can_enable_segment_mode(job)) {
        clear_progress_line(job);
        job->pending_segment_enable = true;
        request_close(job, c, false);
        job->schedule_deferred = true;
        return;
    }

    if (slot != NULL && job->step == DL_STEP_CHUNK && slot->chunk_expected == 0) {
        slot->chunk_expected = slot->range_end - slot->range_start + 1U;
    } else if (slot == NULL && job->step == DL_STEP_CHUNK && job->chunk_expected == 0) {
        job->chunk_expected = job->active_range_end - job->active_range_start + 1U;
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

        if (!job->failed) {
            const uint64_t now = mg_millis();
            if (job->segment_mode && slot != NULL && slot->in_use && slot->request_sent) {
                if (now - slot->last_activity_ms > DL_IDLE_TIMEOUT_MS) {
                    mg_error(c, "Segment stalled");
                    LOG_ERROR("Segment stalled, jobId: %s", job->item_id);
                }
            } else if (!job->segment_mode && now - job->last_activity_ms > DL_IDLE_TIMEOUT_MS) {
                mg_error(c, "Download stalled");
                LOG_ERROR("Download stalled, jobId: %s", job->item_id);
            }
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

        if (ctx->use_proxy && !ctx->proxy_tunnel_done) {
            if (ctx->proxy_kind == ProxyKindSocks5) {
                proxy_socks5_begin(c, job->current_url, ctx->proxy_url, &ctx->socks);
                return;
            }
            if (proxy_target_is_https(job->current_url)) {
                proxy_send_connect(c, job->current_url, ctx->proxy_url);
                return;
            }
            send_request_for_slot(job, c, slot);
            return;
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
        const uint64_t range_start = slot != NULL ? slot->range_start : job->active_range_start;
        const uint64_t range_end = slot != NULL ? slot->range_end : job->active_range_end;

        if (streaming || headers_received) {
            return;
        }

        if (job->step != DL_STEP_CHUNK) {
            return;
        }

        if (hm->body.len == 0) {
            return;
        }

        if (!write_at_offset(job, hm->body.buf, hm->body.len, range_start)) {
            request_close(job, c, false);
            return;
        }

        sync_temp_file(job);
        if (job->mutex != NULL) {
            avar_mutex_lock(job->mutex);
        }
        (void)download_state_mark_range_done(job->state, range_start, range_end);
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
        if (ctx->use_proxy && !ctx->proxy_tunnel_done && c->recv.len > 0) {
            if (ctx->proxy_kind == ProxyKindSocks5) {
                if (proxy_socks5_handle_read((const char *)c->recv.buf, c->recv.len, c,
                                             job->current_url, ctx->proxy_url, &ctx->socks)) {
                    ctx->proxy_tunnel_done = true;
                    c->recv.len = 0;
                    if (proxy_target_is_https(job->current_url)) {
                        init_download_tls(c, job->current_url);
                    } else {
                        send_request_for_slot(job, c, slot);
                    }
                    return;
                }
                if (ctx->socks.stage == ProxySocksStageFailed) {
                    set_error(job, "SOCKS5 proxy failed");
                    request_close(job, c, false);
                    return;
                }
                return;
            }

            if (proxy_connect_response_ok((const char *)c->recv.buf, c->recv.len)) {
                ctx->proxy_tunnel_done = true;
                c->recv.len = 0;
                if (proxy_target_is_https(job->current_url)) {
                    init_download_tls(c, job->current_url);
                } else {
                    send_request_for_slot(job, c, slot);
                }
                return;
            }
            if (c->recv.len >= 12U) {
                set_error(job, "Proxy CONNECT failed");
                request_close(job, c, false);
                return;
            }
            return;
        }

        const bool streaming = slot != NULL ? slot->streaming : job->streaming;
        if (streaming && c->recv.len > 0) {
            if (slot != NULL) {
                if (!slot_write_body(job, slot, c->recv.buf, c->recv.len)) {
                    request_close(job, c, false);
                    return;
                }
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
                /* The matching MG_EV_CLOSE drives the retry/budget bookkeeping so
                 * a single failure is not counted twice. */
                LOG_DEBUG("Segment %llu-%llu connection error",
                          (unsigned long long)slot->range_start,
                          (unsigned long long)slot->range_end);
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

static bool prepare_chunk_download(DownloadJob *job) {
    if (job == NULL || job->state == NULL || job->state->total_size == 0U) {
        return false;
    }

    if (segment_should_enable(&job->seg_cfg, job->state->total_size, job->use_ranges)
        && download_state_init_chunks(job->state, job->state->total_size, job->seg_cfg.chunk_size)
               != 0) {
        return false;
    }

    if (download_state_all_chunks_done(job->state)) {
        return false;
    }

    job->step = DL_STEP_CHUNK;
    if (segment_should_enable(&job->seg_cfg, job->state->total_size, job->use_ranges)) {
        job->segment_mode = true;
    }

    uint64_t start = 0U;
    uint64_t end = 0U;
    if (!next_pending_segment(job, &start, &end)) {
        return false;
    }

    job->active_range_start = start;
    job->active_range_end = end;
    return true;
}

static char *hls_output_filename(const char *filename) {
    if (filename == NULL) {
        return NULL;
    }

    const char *ext = strrchr(filename, '.');
    if (ext != NULL && strcasecmp(ext, ".m3u8") == 0) {
        const size_t prefix = (size_t)(ext - filename);
        char *out = malloc(prefix + 4U);
        if (out == NULL) {
            return NULL;
        }
        snprintf(out, prefix + 4U, "%.*s.ts", (int)prefix, filename);
        return out;
    }

    char *out = malloc(strlen(filename) + 4U);
    if (out == NULL) {
        return NULL;
    }
    snprintf(out, strlen(filename) + 4U, "%s.ts", filename);
    return out;
}

static int run_hls_download(DownloadJob *job) {
    if (job == NULL || job->url == NULL || job->temp_path == NULL) {
        return EXIT_FAILURE;
    }

    job->failed = false;
    job->done = false;
    (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);

    const char *referer = job->state != NULL ? job->state->referer : NULL;
    if (stream_hls_download(job->url, job->temp_path, referer) != 0) {
        set_error(job, "HLS download failed");
        (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
        job->failed = true;
        job->done = true;
        return EXIT_FAILURE;
    }

    if (!finalize_download(job)) {
        job->failed = true;
        return EXIT_FAILURE;
    }

    fputc('\n', stderr);
    report_cli(job);
    return EXIT_SUCCESS;
}

static int run_download(DownloadJob *job) {
    if (stream_url_is_hls(job->url, job->state != NULL ? job->state->stream_kind : NULL)) {
        return run_hls_download(job);
    }

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

    job->step = DL_STEP_STREAM;
    job->segment_mode = false;
    if (!prepare_chunk_download(job)) {
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
    active_jobs_register(job);
#if defined(AVAR_TESTING)
    const uint64_t test_deadline_ms = mg_millis() + AVAR_TEST_DOWNLOAD_MAX_MS;
#endif
    while (!job->done && !job->failed) {
#if defined(AVAR_TESTING)
        if (mg_millis() > test_deadline_ms) {
            set_error(job, "Download exceeded test time limit");
            break;
        }
#endif
        if (job->cancel_requested) {
            job->done = true;
            (void)dm_item_upsert(job, AVAR_DL_STATUS_STOPPED);
            break;
        }
        if (job->pause_requested) {
            job->done = true;
            (void)dm_item_upsert(job, AVAR_DL_STATUS_PAUSED);
            break;
        }
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

    active_jobs_unregister(job);
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

static int run_transient_download_ex(const char *url, const char *queue, const char *name,
                                     const char *proxy_override, const bool start_immediately,
                                     const bool blocking_wait, char **id_out,
                                     const char *stream_kind, const char *referer,
                                     const bool force_new_id, const char *preset_item_id) {
    char *normalized_url = url != NULL ? strdup(url) : NULL;
    if (normalized_url == NULL) {
        LOG_ERROR("Out of memory");
        return EXIT_FAILURE;
    }
    trim_whitespace_inplace(normalized_url);

    if (!is_valid_http_url(normalized_url)) {
        free(normalized_url);
        LOG_ERROR("Invalid download URL");
        return EXIT_FAILURE;
    }

    url = normalized_url;

    char *temp_dir = default_temp_path();
    char *download_dir = default_download_path();
    if (temp_dir == NULL || download_dir == NULL) {
        free(normalized_url);
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to resolve download directories");
        return EXIT_FAILURE;
    }

    if (make_dirs_in_path(temp_dir) != 0 || make_dirs_in_path(download_dir) != 0) {
        free(normalized_url);
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to create download directories");
        return EXIT_FAILURE;
    }

    char *item_id = NULL;
    if (!force_new_id) {
        item_id = find_resumable_item_id(url);
    }
    if (item_id == NULL) {
        if (preset_item_id != NULL && preset_item_id[0] != '\0') {
            item_id = strdup(preset_item_id);
        } else {
            item_id = download_generate_id();
        }
    }

    if (item_id == NULL) {
        free(normalized_url);
        free(temp_dir);
        free(download_dir);
        LOG_ERROR("Failed to allocate download id");
        return EXIT_FAILURE;
    }

    char *job_dir = build_job_dir(temp_dir, item_id);
    char *state_path = job_dir != NULL ? build_state_path(job_dir) : NULL;
    if (job_dir == NULL || state_path == NULL || make_dirs_in_path(job_dir) != 0) {
        free(normalized_url);
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
        bool filename_inferred = true;
        if (name != NULL && name[0] != '\0') {
            filename = sanitize_filename(name);
            filename_inferred = false;
        } else {
            filename = choose_filename(url, NULL, 0);
        }
        if (stream_url_is_hls(url, stream_kind) && filename != NULL) {
            char *ts_name = hls_output_filename(filename);
            if (ts_name != NULL) {
                free(filename);
                filename = ts_name;
            }
        }
        if (filename == NULL) {
            free(normalized_url);
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
            state->filename_inferred = filename_inferred;
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
            if (stream_kind != NULL && stream_kind[0] != '\0') {
                free(state->stream_kind);
                state->stream_kind = strdup(stream_kind);
            } else if (stream_url_is_hls(url, NULL)) {
                free(state->stream_kind);
                state->stream_kind = strdup("hls");
            }
            if (referer != NULL && referer[0] != '\0') {
                free(state->referer);
                state->referer = strdup(referer);
            }
        }
    }

    apply_proxy_to_state(state, proxy_override);
    if (state != NULL && state_path != NULL) {
        (void)download_state_save(state, state_path);
    }

    free(temp_dir);
    free(download_dir);
    free(job_dir);

    if (state == NULL || filename == NULL || temp_path == NULL || dest_path == NULL) {
        download_state_free(state);
        free(normalized_url);
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
        free(normalized_url);
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
    free(normalized_url);
    job->item_id = item_id;
    job->filename = filename;
    job->filename_inferred = state != NULL ? state->filename_inferred : true;
    job->temp_path = temp_path;
    job->dest_path = dest_path;
    job->state_path = state_path;
    job->state = state;
    job->use_ranges = true;
    job->segment_disabled = false;
    job->last_progress_ms = mg_millis();
    job->last_speed_sample_ms = mg_millis();
    load_job_retry_state(job);

    int rc = EXIT_SUCCESS;
    if (start_immediately) {
        LOG_INFO("Downloading %s <- %s", dest_path, url);
        (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);
        if (blocking_wait) {
            char *spawn_id = strdup(item_id);
            job_free(job);
            job = NULL;
            if (spawn_id == NULL) {
                return EXIT_FAILURE;
            }
            if (spawn_download_by_id(spawn_id, proxy_override) != EXIT_SUCCESS) {
                free(spawn_id);
                return EXIT_FAILURE;
            }
            rc = wait_for_download_item(spawn_id);
            free(spawn_id);
        } else {
            rc = run_download(job);
            if (rc != EXIT_SUCCESS && !job->failed) {
                (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
            }
            job_free(job);
            job = NULL;
        }
    } else {
        LOG_INFO("Queued %s <- %s", dest_path, url);
        (void)dm_item_upsert(job, AVAR_DL_STATUS_QUEUED);
        if (id_out != NULL && job->item_id != NULL) {
            *id_out = strdup(job->item_id);
            if (*id_out == NULL) {
                rc = EXIT_FAILURE;
            }
        }
    }

    if (job != NULL) {
        job_free(job);
    }
    return rc;
}

static int run_transient_download(const char *url, const char *queue, const char *name,
                                  const char *proxy_override, const char *preset_item_id) {
    return run_transient_download_ex(url, queue, name, proxy_override, true, false, NULL, NULL, NULL,
                                     false, preset_item_id);
}

int download_enqueue_with_proxy(const char *url, const char *queue, const char *name,
                                const char *proxy_url, char **id_out) {
    return download_enqueue_ex(url, queue, name, proxy_url, NULL, NULL, false, id_out);
}

int download_enqueue_ex(const char *url, const char *queue, const char *name,
                        const char *proxy_url, const char *stream_kind, const char *referer,
                        const bool force_new_id, char **id_out) {
    return run_transient_download_ex(url, queue, name, proxy_url, false, false, id_out, stream_kind,
                                     referer, force_new_id, NULL);
}

int transient_download(const char *url, const char *queue, const char *name, const char *proxy_url,
                       const bool attached) {
    if (!attached) {
        LOG_ERROR("Only --attached downloads are supported in local CLI mode");
        return EXIT_FAILURE;
    }
    return run_transient_download_ex(url, queue, name, proxy_url, true, true, NULL, NULL, NULL, false,
                                     NULL);
}

static void background_download_task(void *arg) {
    BackgroundDownloadArgs *args = (BackgroundDownloadArgs *)arg;
    if (args == NULL) {
        return;
    }

    atomic_fetch_add(&g_active_downloads, 1U);
    (void)run_transient_download_ex(args->url, args->queue, args->name, args->proxy, true, false,
                                    NULL, NULL, NULL, false, args->item_id);
    atomic_fetch_sub(&g_active_downloads, 1U);
    background_args_free(args);
}

int download_start_background(const char *url, const char *queue, const char *name, char **id_out) {
    return download_start_background_with_proxy(url, queue, name, NULL, id_out);
}

int download_start_background_with_proxy(const char *url, const char *queue, const char *name,
                                         const char *proxy_url, char **id_out) {
    char *normalized_url = url != NULL ? strdup(url) : NULL;
    if (normalized_url == NULL) {
        return EXIT_FAILURE;
    }
    trim_whitespace_inplace(normalized_url);

    if (!is_valid_http_url(normalized_url)) {
        free(normalized_url);
        return EXIT_FAILURE;
    }

    BackgroundDownloadArgs *args = calloc(1, sizeof(*args));
    if (args == NULL) {
        free(normalized_url);
        return EXIT_FAILURE;
    }

    args->url = normalized_url;
    args->queue = queue != NULL ? strdup(queue) : NULL;
    args->name = name != NULL ? strdup(name) : NULL;
    args->proxy = proxy_url != NULL ? strdup(proxy_url) : NULL;
    if (args->url == NULL || (queue != NULL && args->queue == NULL) ||
        (name != NULL && args->name == NULL) || (proxy_url != NULL && args->proxy == NULL)) {
        background_args_free(args);
        return EXIT_FAILURE;
    }

    if (id_out != NULL) {
        args->item_id = download_generate_id();
        if (args->item_id == NULL) {
            background_args_free(args);
            return EXIT_FAILURE;
        }
        *id_out = strdup(args->item_id);
        if (*id_out == NULL) {
            background_args_free(args);
            return EXIT_FAILURE;
        }
    }

    if (!download_pool_submit(background_download_task, args)) {
        background_args_free(args);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int find_download_item_index(const char *target, const bool by_id) {
    if (target == NULL) {
        return -1;
    }

    const char *field = by_id ? AVAR_FIELD_ID : AVAR_FIELD_FILENAME;
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *value = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, field);
        if (value != NULL && strcmp(value, target) == 0) {
            free(value);
            return (int)i;
        }
        free(value);
    }

    return -1;
}

static uint32_t download_snowflake_machine_id(void) {
#if defined(_WIN32)
    const unsigned long pid = (unsigned long)GetCurrentProcessId();
#else
    const unsigned long pid = (unsigned long)getpid();
#endif
    return (uint32_t)(pid & DL_SNOWFLAKE_MACHINE_MASK);
}

static uint64_t download_snowflake_mix(const uint64_t value) {
    uint64_t mixed = value;

    mixed ^= mixed >> 30;
    mixed *= 0xbf58476d1ce4e5b9ULL;
    mixed ^= mixed >> 27;
    mixed *= 0x94d049bb133111ebULL;
    mixed ^= mixed >> 31;
    return mixed;
}

static uint64_t download_snowflake_wait_next_ms(const uint64_t last_ms) {
    uint64_t now = (uint64_t)mg_millis();
    while (now <= last_ms) {
        platform_sleep_ms(1U);
        now = (uint64_t)mg_millis();
    }
    return now;
}

static uint64_t download_snowflake_next(void) {
    if (g_download_snowflake.mutex == NULL) {
        g_download_snowflake.mutex = avar_mutex_create();
        g_download_snowflake.machine_id = download_snowflake_machine_id();
    }

    avar_mutex_lock(g_download_snowflake.mutex);

    uint64_t now = (uint64_t)mg_millis();
    if (now < g_download_snowflake.last_ms) {
        now = g_download_snowflake.last_ms;
    }

    if (now == g_download_snowflake.last_ms) {
        g_download_snowflake.sequence++;
        if (g_download_snowflake.sequence > DL_SNOWFLAKE_SEQUENCE_MASK) {
            now = download_snowflake_wait_next_ms(g_download_snowflake.last_ms);
            g_download_snowflake.sequence = 0U;
        }
    } else {
        g_download_snowflake.sequence = 0U;
    }

    g_download_snowflake.last_ms = now;

    const uint64_t timestamp = now - DL_SNOWFLAKE_EPOCH_MS;
    const uint64_t raw =
        (timestamp << DL_SNOWFLAKE_TIMESTAMP_SHIFT)
        | ((uint64_t)g_download_snowflake.machine_id << DL_SNOWFLAKE_MACHINE_SHIFT)
        | (uint64_t)g_download_snowflake.sequence;

    avar_mutex_unlock(g_download_snowflake.mutex);
    return raw;
}

static char *download_generate_id(void) {
    char generated[AVAR_DL_ID_BUF_SIZE];

    for (unsigned attempt = 0U; attempt < 32U; ++attempt) {
        const uint64_t mixed = download_snowflake_mix(download_snowflake_next());
        snprintf(generated, sizeof generated, "%s%016llx", AVAR_DL_ID_PREFIX,
                 (unsigned long long)mixed);

        if (find_download_item_index(generated, true) < 0) {
            return strdup(generated);
        }
    }

    LOG_ERROR("Failed to generate unique download id");
    return NULL;
}

static void log_download_item_status(const char *item_id, const char *filename, const char *status) {
    const char *label = filename != NULL && filename[0] != '\0' ? filename : item_id;
    if (label == NULL || status == NULL) {
        return;
    }

    if (strcmp(status, AVAR_DL_STATUS_COMPLETED) == 0) {
        LOG_INFO("Download completed: %s", label);
    } else if (strcmp(status, AVAR_DL_STATUS_STOPPED) == 0) {
        LOG_INFO("Download stopped: %s", label);
    } else if (strcmp(status, AVAR_DL_STATUS_PAUSED) == 0) {
        LOG_INFO("Download paused: %s", label);
    } else if (strcmp(status, AVAR_DL_STATUS_FAILED) == 0) {
        LOG_WARNING("Download failed: %s", label);
    }
}

static void log_job_status(const DownloadJob *job, const char *status) {
    if (job == NULL) {
        return;
    }
    log_download_item_status(job->item_id, job->filename, status);
}

static int update_download_item_status(const char *item_id, const char *status) {
    if (item_id == NULL || status == NULL) {
        return -1;
    }

    const int index = find_download_item_index(item_id, true);
    if (index < 0) {
        return -1;
    }

    char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, (size_t)index);
    if (json == NULL) {
        return -1;
    }

    cJSON *obj = cJSON_Parse(json);
    free(json);
    if (obj == NULL) {
        return -1;
    }

    const cJSON *filename_node = cJSON_GetObjectItemCaseSensitive(obj, AVAR_FIELD_FILENAME);
    const char *filename =
            cJSON_IsString(filename_node) && filename_node->valuestring != NULL
                    ? filename_node->valuestring
                    : NULL;

    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_STATUS);
    cJSON_AddStringToObject(obj, AVAR_FIELD_STATUS, status);

    char *updated = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (updated == NULL) {
        return -1;
    }

    const int rc = replace_config_array_item_at(AVAR_CFG_DM_ITEMS, (size_t)index, updated);
    cJSON_free(updated);
    if (rc == 0) {
        log_download_item_status(item_id, filename, status);
    }
    return rc;
}

static void apply_proxy_to_state(DownloadState *state, const char *proxy_override) {
    if (state == NULL) {
        return;
    }

    char *proxy_url = proxy_resolve_for_target(state->url, proxy_override);
    if (proxy_override != NULL && proxy_override[0] != '\0') {
        free(state->proxy);
        state->proxy = proxy_url;
        return;
    }

    if (state->proxy == NULL && proxy_url != NULL) {
        state->proxy = proxy_url;
    } else {
        free(proxy_url);
    }
}

static void active_jobs_register(DownloadJob *job) {
    if (job == NULL || job->item_id == NULL) {
        return;
    }

    if (g_active_jobs.mutex == NULL) {
        g_active_jobs.mutex = avar_mutex_create();
    }

    avar_mutex_lock(g_active_jobs.mutex);
    for (size_t i = 0; i < AVAR_ACTIVE_JOB_CAP; ++i) {
        if (g_active_jobs.jobs[i] == NULL) {
            g_active_jobs.jobs[i] = job;
            break;
        }
    }
    avar_mutex_unlock(g_active_jobs.mutex);
}

static void active_jobs_unregister(DownloadJob *job) {
    if (job == NULL || g_active_jobs.mutex == NULL) {
        return;
    }

    avar_mutex_lock(g_active_jobs.mutex);
    for (size_t i = 0; i < AVAR_ACTIVE_JOB_CAP; ++i) {
        if (g_active_jobs.jobs[i] == job) {
            g_active_jobs.jobs[i] = NULL;
            break;
        }
    }
    avar_mutex_unlock(g_active_jobs.mutex);
}

static DownloadJob *active_jobs_find(const char *item_id) {
    if (item_id == NULL || g_active_jobs.mutex == NULL) {
        return NULL;
    }

    DownloadJob *found = NULL;
    avar_mutex_lock(g_active_jobs.mutex);
    for (size_t i = 0; i < AVAR_ACTIVE_JOB_CAP; ++i) {
        DownloadJob *job = g_active_jobs.jobs[i];
        if (job != NULL && job->item_id != NULL && strcmp(job->item_id, item_id) == 0) {
            found = job;
            break;
        }
    }
    avar_mutex_unlock(g_active_jobs.mutex);
    return found;
}

static void cleanup_download_artifacts(const char *item_id, const bool purge_dest) {
    if (item_id == NULL) {
        return;
    }

    char *temp_dir = default_temp_path();
    if (temp_dir == NULL) {
        return;
    }

    char *job_dir = build_job_dir(temp_dir, item_id);
    char *state_path = job_dir != NULL ? build_state_path(job_dir) : NULL;
    if (state_path != NULL) {
        DownloadState *state = download_state_load(state_path);
        if (state != NULL) {
            if (purge_dest && state->dest_path != NULL) {
                remove(state->dest_path);
            }
            if (state->temp_path != NULL) {
                remove(state->temp_path);
            }
            download_state_free(state);
        }
    }

    remove_job_work_dir(state_path);
    free(state_path);
    free(job_dir);
    free(temp_dir);
}

static int run_download_for_item_id(const char *item_id, const char *proxy_override) {
    if (item_id == NULL) {
        return EXIT_FAILURE;
    }

    const int index = find_download_item_index(item_id, true);
    if (index < 0) {
        LOG_ERROR("Download item not found: %s", item_id);
        return EXIT_FAILURE;
    }

    char *url = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_URL);
    char *queue_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_QUEUE_ID);
    if (url == NULL || !is_valid_http_url(url)) {
        free(url);
        free(queue_id);
        LOG_ERROR("Invalid download URL for item %s", item_id);
        return EXIT_FAILURE;
    }

    char *temp_dir = default_temp_path();
    char *download_dir = default_download_path();
    if (temp_dir == NULL || download_dir == NULL) {
        free(url);
        free(queue_id);
        free(temp_dir);
        free(download_dir);
        return EXIT_FAILURE;
    }

    (void)make_dirs_in_path(temp_dir);
    (void)make_dirs_in_path(download_dir);

    char *job_dir = build_job_dir(temp_dir, item_id);
    char *state_path = job_dir != NULL ? build_state_path(job_dir) : NULL;
    if (job_dir == NULL || state_path == NULL || make_dirs_in_path(job_dir) != 0) {
        free(url);
        free(queue_id);
        free(temp_dir);
        free(download_dir);
        free(job_dir);
        free(state_path);
        return EXIT_FAILURE;
    }

    DownloadState *state = download_state_load(state_path);
    char *filename = NULL;
    char *temp_path = NULL;
    char *dest_path = NULL;

    if (state != NULL) {
        filename = strdup(state->filename);
        temp_path = strdup(state->temp_path);
        dest_path = strdup(state->dest_path);
    } else {
        filename = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_FILENAME);
        if (filename == NULL) {
            filename = choose_filename(url, NULL, 0);
        }
        char *stream_kind =
                get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STREAM_KIND);
        if (stream_url_is_hls(url, stream_kind) && filename != NULL) {
            char *ts_name = hls_output_filename(filename);
            if (ts_name != NULL) {
                free(filename);
                filename = ts_name;
            }
        }
        temp_path = path_join(job_dir, filename);
        dest_path = path_join(download_dir, filename);
        state = download_state_create(url, filename, temp_path, dest_path, 0, DL_CHUNK_SIZE);
        if (state != NULL) {
            char *inferred = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index,
                                                         AVAR_FIELD_FILENAME_INFERRED);
            state->filename_inferred =
                    inferred == NULL || strcmp(inferred, "true") == 0 || strcmp(inferred, "1") == 0;
            free(inferred);
            state->id = strdup(item_id);
            state->queued_at = format_datetime_iso();
            state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
            if (queue_id != NULL) {
                state->queue_id = strdup(queue_id);
            }
            if (stream_kind != NULL && stream_kind[0] != '\0') {
                state->stream_kind = stream_kind;
                stream_kind = NULL;
            } else if (stream_url_is_hls(url, NULL)) {
                state->stream_kind = strdup("hls");
            }
            char *referer =
                    get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_REFERER);
            if (referer != NULL && referer[0] != '\0') {
                state->referer = referer;
                referer = NULL;
            }
        }
        free(stream_kind);
    }

    apply_proxy_to_state(state, proxy_override);

    free(temp_dir);
    free(download_dir);
    free(job_dir);
    free(queue_id);

    if (state == NULL || filename == NULL || temp_path == NULL || dest_path == NULL) {
        download_state_free(state);
        free(url);
        free(filename);
        free(temp_path);
        free(dest_path);
        free(state_path);
        return EXIT_FAILURE;
    }

    DownloadJob *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        download_state_free(state);
        free(url);
        free(filename);
        free(temp_path);
        free(dest_path);
        free(state_path);
        return EXIT_FAILURE;
    }

    char *last_try_at = format_datetime_iso();
    if (last_try_at != NULL) {
        free(state->last_try_at);
        state->last_try_at = last_try_at;
    }

    job->url = url;
    job->item_id = strdup(item_id);
    job->filename = filename;
    job->filename_inferred = state->filename_inferred;
    job->temp_path = temp_path;
    job->dest_path = dest_path;
    job->state_path = state_path;
    job->state = state;
    job->use_ranges = true;
    load_job_retry_state(job);

    LOG_INFO("Downloading %s <- %s", dest_path, url);
    (void)dm_item_upsert(job, AVAR_DL_STATUS_DOWNLOADING);

    const int rc = run_download(job);
    if (rc != EXIT_SUCCESS && !job->failed && !job->cancel_requested && !job->pause_requested) {
        (void)dm_item_upsert(job, AVAR_DL_STATUS_FAILED);
    }
    job_free(job);
    return rc;
}

static void background_download_task_by_id(void *arg);

static int spawn_download_by_id(const char *item_id, const char *proxy_override) {
    BackgroundDownloadArgs *args = calloc(1, sizeof(*args));
    if (args == NULL) {
        return EXIT_FAILURE;
    }

    args->item_id = strdup(item_id);
    args->proxy = proxy_override != NULL ? strdup(proxy_override) : NULL;
    if (args->item_id == NULL) {
        background_args_free(args);
        return EXIT_FAILURE;
    }

    if (!download_pool_submit(background_download_task_by_id, args)) {
        background_args_free(args);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void background_download_task_by_id(void *arg) {
    BackgroundDownloadArgs *args = (BackgroundDownloadArgs *)arg;
    if (args == NULL || args->item_id == NULL) {
        background_args_free(args);
        return;
    }

    atomic_fetch_add(&g_active_downloads, 1U);
    (void)run_download_for_item_id(args->item_id, args->proxy);
    atomic_fetch_sub(&g_active_downloads, 1U);
    background_args_free(args);
}

static bool download_should_resume_on_startup(const char *status, const char *queue_id) {
    if (status == NULL) {
        return false;
    }

    if (strcmp(status, AVAR_DL_STATUS_DOWNLOADING) == 0) {
        return true;
    }

    if (strcmp(status, AVAR_DL_STATUS_QUEUED) == 0 && queue_id != NULL && queue_id[0] != '\0'
        && queue_is_started(queue_id)) {
        return true;
    }

    return false;
}

void download_resume_interrupted(void) {
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_ID);
        char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_STATUS);
        char *queue_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_QUEUE_ID);

        if (id == NULL || !download_should_resume_on_startup(status, queue_id)) {
            free(id);
            free(status);
            free(queue_id);
            continue;
        }

        free(status);
        free(queue_id);

        if (active_jobs_find(id) != NULL) {
            free(id);
            continue;
        }

        LOG_INFO("Resuming download %s after daemon start", id);
        (void)spawn_download_by_id(id, NULL);
        free(id);
    }
}

int download_pause(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    DownloadJob *job = active_jobs_find(id);
    if (job != NULL) {
        job->pause_requested = true;
        return EXIT_SUCCESS;
    }

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STATUS);
    if (status == NULL || strcmp(status, AVAR_DL_STATUS_DOWNLOADING) != 0) {
        free(status);
        return EXIT_FAILURE;
    }
    free(status);

    return update_download_item_status(id, AVAR_DL_STATUS_PAUSED) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int download_resume(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STATUS);
    if (status == NULL || strcmp(status, AVAR_DL_STATUS_PAUSED) != 0) {
        free(status);
        return EXIT_FAILURE;
    }
    free(status);

    return spawn_download_by_id(id, NULL);
}

int download_start(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STATUS);
    if (status == NULL) {
        return EXIT_FAILURE;
    }

    const bool startable = strcmp(status, AVAR_DL_STATUS_QUEUED) == 0 ||
                           strcmp(status, AVAR_DL_STATUS_PAUSED) == 0 ||
                           strcmp(status, AVAR_DL_STATUS_FAILED) == 0 ||
                           strcmp(status, AVAR_DL_STATUS_STOPPED) == 0;
    free(status);

    if (!startable) {
        return EXIT_FAILURE;
    }

    return spawn_download_by_id(id, NULL);
}

int download_stop(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    DownloadJob *job = active_jobs_find(id);
    if (job != NULL) {
        job->cancel_requested = true;
        return EXIT_SUCCESS;
    }

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    return update_download_item_status(id, AVAR_DL_STATUS_STOPPED) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static char *download_item_state_path(const char *item_id) {
    if (item_id == NULL) {
        return NULL;
    }

    char *temp_dir = default_temp_path();
    if (temp_dir == NULL) {
        return NULL;
    }

    char *job_dir = build_job_dir(temp_dir, item_id);
    free(temp_dir);
    if (job_dir == NULL) {
        return NULL;
    }

    char *state_path = build_state_path(job_dir);
    free(job_dir);
    return state_path;
}

static int clear_download_item_description(const char *item_id) {
    if (item_id == NULL) {
        return -1;
    }

    const int index = find_download_item_index(item_id, true);
    if (index < 0) {
        return -1;
    }

    char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, (size_t)index);
    if (json == NULL) {
        return -1;
    }

    cJSON *obj = cJSON_Parse(json);
    free(json);
    if (obj == NULL) {
        return -1;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_DESCRIPTION);
    cJSON_AddNullToObject(obj, AVAR_FIELD_DESCRIPTION);

    char *updated = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (updated == NULL) {
        return -1;
    }

    const int rc = replace_config_array_item_at(AVAR_CFG_DM_ITEMS, (size_t)index, updated);
    cJSON_free(updated);
    return rc;
}

static int reset_download_progress_for_restart(const char *item_id) {
    if (item_id == NULL) {
        return -1;
    }

    char *state_path = download_item_state_path(item_id);
    DownloadState *state = state_path != NULL ? download_state_load(state_path) : NULL;
    if (state != NULL) {
        if (state->temp_path != NULL) {
            (void)remove(state->temp_path);
        }
        reset_chunk_progress(state);
        state->bytes_downloaded = 0U;
        state->total_size = 0U;
        free(state->etag);
        state->etag = NULL;
        free(state->last_modified);
        state->last_modified = NULL;
        free(state->description);
        state->description = NULL;
        (void)download_state_save(state, state_path);
        download_state_free(state);
    }
    free(state_path);

    const int index = find_download_item_index(item_id, true);
    if (index < 0) {
        return -1;
    }

    char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, (size_t)index);
    if (json == NULL) {
        return -1;
    }

    cJSON *obj = cJSON_Parse(json);
    free(json);
    if (obj == NULL) {
        return -1;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_BYTES_DOWNLOADED);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_BYTES_DOWNLOADED, 0.0);
    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_TOTAL_BYTES);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_TOTAL_BYTES, 0.0);
    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_ERROR_COUNT);
    cJSON_AddNumberToObject(obj, AVAR_FIELD_ERROR_COUNT, 0.0);
    cJSON_DeleteItemFromObjectCaseSensitive(obj, AVAR_FIELD_DESCRIPTION);
    cJSON_AddNullToObject(obj, AVAR_FIELD_DESCRIPTION);

    char *updated = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (updated == NULL) {
        return -1;
    }

    const int rc = replace_config_array_item_at(AVAR_CFG_DM_ITEMS, (size_t)index, updated);
    cJSON_free(updated);
    return rc;
}

int download_restart(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    DownloadJob *job = active_jobs_find(id);
    if (job != NULL) {
        job->cancel_requested = true;
    }

    if (reset_download_progress_for_restart(id) != 0) {
        return EXIT_FAILURE;
    }

    return spawn_download_by_id(id, NULL);
}

int download_dismiss_resume_prompt(const char *id) {
    if (id == NULL) {
        return EXIT_FAILURE;
    }

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    char *description =
            get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_DESCRIPTION);
    const bool resume_prompt = description != NULL
                               && strcmp(description, AVAR_DL_DESC_RESUME_UNSUPPORTED) == 0;
    free(description);
    if (!resume_prompt) {
        return EXIT_FAILURE;
    }

    char *state_path = download_item_state_path(id);
    if (state_path != NULL) {
        DownloadState *state = download_state_load(state_path);
        if (state != NULL) {
            free(state->description);
            state->description = NULL;
            (void)download_state_save(state, state_path);
            download_state_free(state);
        }
        free(state_path);
    }

    return clear_download_item_description(id) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void purge_download_files(const char *item_id) {
    cleanup_download_artifacts(item_id, true);
}

int download_resolve_dest_path(const char *id, char **path_out) {
    if (id == NULL || path_out == NULL) {
        return EXIT_FAILURE;
    }
    *path_out = NULL;

    const int index = find_download_item_index(id, true);
    if (index < 0) {
        return EXIT_FAILURE;
    }

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_STATUS);
    if (status == NULL || strcmp(status, AVAR_DL_STATUS_COMPLETED) != 0) {
        free(status);
        return EXIT_FAILURE;
    }
    free(status);

    char *temp_dir = default_temp_path();
    char *job_dir = temp_dir != NULL ? build_job_dir(temp_dir, id) : NULL;
    char *state_path = job_dir != NULL ? build_state_path(job_dir) : NULL;
    DownloadState *state = state_path != NULL ? download_state_load(state_path) : NULL;

    char *resolved = NULL;
    if (state != NULL && state->dest_path != NULL && state->dest_path[0] != '\0') {
        resolved = strdup(state->dest_path);
    }

    if (state != NULL) {
        download_state_free(state);
    }
    free(state_path);
    free(job_dir);
    free(temp_dir);

    if (resolved == NULL) {
        char *filename =
            get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_FILENAME);
        char *download_dir = default_download_path();
        if (filename == NULL || download_dir == NULL) {
            free(filename);
            free(download_dir);
            return EXIT_FAILURE;
        }
        resolved = path_join(download_dir, filename);
        free(filename);
        free(download_dir);
        if (resolved == NULL) {
            return EXIT_FAILURE;
        }
    }

    if (!file_exists(resolved)) {
        free(resolved);
        return EXIT_FAILURE;
    }

    *path_out = resolved;
    return EXIT_SUCCESS;
}

int download_remove(const char *target, const bool by_id, const bool purge_files, const bool force) {
    (void)force;

    const int index = find_download_item_index(target, by_id);
    if (index < 0) {
        LOG_ERROR("Download item not found: %s", target);
        return EXIT_FAILURE;
    }

    char *item_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, (size_t)index, AVAR_FIELD_ID);
    if (item_id == NULL) {
        return EXIT_FAILURE;
    }

    DownloadJob *job = active_jobs_find(item_id);
    if (job != NULL) {
        job->cancel_requested = true;
    }

    cleanup_download_artifacts(item_id, purge_files);

    const int rc = remove_config_array_item(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, item_id);
    free(item_id);
    if (rc != 0) {
        LOG_ERROR("Failed to remove download item from config");
        return EXIT_FAILURE;
    }

    LOG_INFO("Removed download item %s (%s files)", target, purge_files ? "purged" : "kept");
    return EXIT_SUCCESS;
}

#if defined(AVAR_TESTING)
char *download_test_choose_filename(const char *url, const char *header_value,
                                     const size_t header_len) {
    return choose_filename(url, header_value, header_len);
}

bool download_test_parse_content_range_total(const char *header, uint64_t *total_out) {
    if (header == NULL) {
        return false;
    }
    const struct mg_str value = mg_str(header);
    return parse_content_range_total(value, total_out);
}

uint64_t download_test_existing_file_size(const char *path) {
    return existing_file_size(path);
}

char *download_test_generate_id(void) {
    return download_generate_id();
}
#endif
