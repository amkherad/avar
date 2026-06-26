#include <cJSON.h>

#include <cli.h>
#include <config.h>
#include <daemon/daemon.h>
#include <daemon/daemon_rpc.h>
#include <daemon/daemon_session.h>
#include <daemon/daemon_transport.h>
#include <download.h>
#include <file_checksum.h>
#include <file-system.h>
#include <http_proxy.h>
#include <queue.h>
#include <system_stats.h>
#include <logger.h>
#include <mongoose.h>
#include <queue.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <io.h>
    #include <fcntl.h>
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#define AVAR_DAEMON_LOG_RING_CAP 256U
#define AVAR_DAEMON_LOG_LINE_MAX 512U

typedef struct {
    char lines[AVAR_DAEMON_LOG_RING_CAP][AVAR_DAEMON_LOG_LINE_MAX];
    uint64_t seq[AVAR_DAEMON_LOG_RING_CAP];
    size_t head;
    size_t count;
    uint64_t next_seq;
} DaemonLogRing;

static DaemonLogRing _log_ring = {0};
static char _auth_token[128] = {0};
static time_t _daemon_started_at = 0;
static time_t _frontend_last_activity = 0;
static time_t _stream_last_tick = 0;
static time_t _stream_last_full_send = 0;
static uint64_t _stream_cached_fingerprint = 0U;
#if defined(_WIN32)
static CRITICAL_SECTION _rpc_lock;
static bool _rpc_lock_ready = false;
#else
static pthread_mutex_t _rpc_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void rpc_lock(void) {
#if defined(_WIN32)
    if (_rpc_lock_ready) {
        EnterCriticalSection(&_rpc_lock);
    }
#else
    (void)pthread_mutex_lock(&_rpc_lock);
#endif
}

static void rpc_unlock(void) {
#if defined(_WIN32)
    if (_rpc_lock_ready) {
        LeaveCriticalSection(&_rpc_lock);
    }
#else
    (void)pthread_mutex_unlock(&_rpc_lock);
#endif
}

static cJSON *rpc_error(int code, const char *message, cJSON *id) {
    cJSON *root = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();
    if (root == NULL || error == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(error);
        return NULL;
    }
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddItemToObject(root, "error", error);
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    if (id != NULL) {
        cJSON_AddItemToObject(root, "id", cJSON_Duplicate(id, true));
    } else {
        cJSON_AddNullToObject(root, "id");
    }
    return root;
}

static cJSON *rpc_result(cJSON *result, cJSON *id) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddItemToObject(root, "result", result != NULL ? result : cJSON_CreateObject());
    if (id != NULL) {
        cJSON_AddItemToObject(root, "id", cJSON_Duplicate(id, true));
    } else {
        cJSON_AddNullToObject(root, "id");
    }
    return root;
}

void daemon_rpc_init(void) {
#if defined(_WIN32)
    if (!_rpc_lock_ready) {
        InitializeCriticalSection(&_rpc_lock);
        _rpc_lock_ready = true;
    }
#endif
    _daemon_started_at = time(NULL);
    _frontend_last_activity = 0;
    _stream_last_tick = 0;
    _stream_last_full_send = 0;
    _stream_cached_fingerprint = 0U;
    memset(&_log_ring, 0, sizeof _log_ring);
}

void daemon_rpc_set_auth_token(const char *token) {
    if (token == NULL) {
        _auth_token[0] = '\0';
        return;
    }
    snprintf(_auth_token, sizeof _auth_token, "%s", token);
}

void daemon_rpc_shutdown(void) {
#if defined(AVAR_TESTING)
    (void)download_wait_idle(500U);
#else
    (void)download_wait_idle(30000U);
#endif
}

void daemon_rpc_log_append(const char *line) {
    if (line == NULL) {
        return;
    }

    _log_ring.seq[_log_ring.head] = _log_ring.next_seq++;
    snprintf(_log_ring.lines[_log_ring.head], AVAR_DAEMON_LOG_LINE_MAX, "%s", line);
    _log_ring.head = (_log_ring.head + 1U) % AVAR_DAEMON_LOG_RING_CAP;
    if (_log_ring.count < AVAR_DAEMON_LOG_RING_CAP) {
        _log_ring.count++;
    }
}

bool daemon_rpc_logs_fetch(const bool follow, const unsigned max_lines, const uint64_t since,
                           char **text_out, uint64_t *next_offset_out) {
    (void)follow;

    if (text_out == NULL) {
        return false;
    }

    const unsigned limit = max_lines == 0U ? 50U : max_lines;
    size_t total = 0;
    size_t matched = 0;
    for (size_t i = 0; i < _log_ring.count; ++i) {
        const size_t idx =
            (_log_ring.head + AVAR_DAEMON_LOG_RING_CAP - _log_ring.count + i) % AVAR_DAEMON_LOG_RING_CAP;
        if (_log_ring.seq[idx] <= since) {
            continue;
        }
        total += strlen(_log_ring.lines[idx]) + 1U;
        matched++;
        if (matched >= limit) {
            break;
        }
    }

    char *buf = calloc(total + 1U, 1U);
    if (buf == NULL) {
        return false;
    }

    size_t offset = 0;
    matched = 0;
    uint64_t last_seq = since;
    for (size_t i = 0; i < _log_ring.count; ++i) {
        const size_t idx =
            (_log_ring.head + AVAR_DAEMON_LOG_RING_CAP - _log_ring.count + i) % AVAR_DAEMON_LOG_RING_CAP;
        if (_log_ring.seq[idx] <= since) {
            continue;
        }
        const size_t len = strlen(_log_ring.lines[idx]);
        memcpy(buf + offset, _log_ring.lines[idx], len);
        offset += len;
        buf[offset++] = '\n';
        last_seq = _log_ring.seq[idx];
        matched++;
        if (matched >= limit) {
            break;
        }
    }

    *text_out = buf;
    if (next_offset_out != NULL) {
        *next_offset_out = last_seq;
    }
    return true;
}

bool daemon_rpc_check_http_auth(struct mg_http_message *hm) {
    if (_auth_token[0] == '\0') {
        return true;
    }
    if (hm == NULL) {
        return false;
    }

    struct mg_str *auth = mg_http_get_header(hm, "Authorization");
    if (auth == NULL) {
        return false;
    }

    char expected[160];
    snprintf(expected, sizeof expected, "Bearer %s", _auth_token);
    return mg_strcmp(*auth, mg_str(expected)) == 0;
}

static int capture_handler_output(int (*handler)(int, char **), int argc, char **argv,
                                  char **output_out) {
    if (output_out != NULL) {
        *output_out = NULL;
    }

#if defined(_WIN32)
    int pipefd[2] = {-1, -1};
    if (_pipe(pipefd, 65536, _O_BINARY) != 0) {
        return handler(argc, argv);
    }
    const int saved_stdout = _dup(_fileno(stdout));
    if (saved_stdout < 0) {
        _close(pipefd[0]);
        _close(pipefd[1]);
        return handler(argc, argv);
    }
    _dup2(pipefd[1], _fileno(stdout));
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return handler(argc, argv);
    }
    const int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return handler(argc, argv);
    }
    dup2(pipefd[1], STDOUT_FILENO);
#endif

    fflush(stdout);
    const int rc = handler(argc, argv);
    fflush(stdout);

#if defined(_WIN32)
    _dup2(saved_stdout, _fileno(stdout));
    _close(saved_stdout);
    _close(pipefd[1]);
#else
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    close(pipefd[1]);
#endif

    char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    char chunk[512];

    for (;;) {
#if defined(_WIN32)
        const int n = (int)_read(pipefd[0], chunk, sizeof chunk);
#else
        const ssize_t n = read(pipefd[0], chunk, sizeof chunk);
#endif
        if (n <= 0) {
            break;
        }
        if (len + (size_t)n + 1U > cap) {
            cap = cap == 0U ? 1024U : cap * 2U;
            char *grown = realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
#if defined(_WIN32)
                _close(pipefd[0]);
#else
                close(pipefd[0]);
#endif
                return rc;
            }
            buf = grown;
        }
        memcpy(buf + len, chunk, (size_t)n);
        len += (size_t)n;
    }

#if defined(_WIN32)
    _close(pipefd[0]);
#else
    close(pipefd[0]);
#endif

    if (buf != NULL) {
        buf[len] = '\0';
    }
    if (output_out != NULL) {
        *output_out = buf;
    } else {
        free(buf);
    }
    return rc;
}

static int dispatch_cli_command(int argc, char **argv, char **output_out) {
    if (argc < 2 || argv == NULL) {
        return EXIT_FAILURE;
    }

    const char *cmd = argv[1];
    if (strcmp(cmd, "download") == 0 || strcmp(cmd, "dl") == 0) {
        return capture_handler_output(handle_download, argc, argv, output_out);
    }
    if (strcmp(cmd, "queue") == 0) {
        return capture_handler_output(handle_queue, argc, argv, output_out);
    }
    if (strcmp(cmd, "config") == 0) {
        return capture_handler_output(handle_config, argc, argv, output_out);
    }

    return EXIT_UNKNOWN_COMMAND;
}

static cJSON *handle_ping(void) {
    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddStringToObject(result, "status", "ok");
    }
    return result;
}

static cJSON *handle_health(void) {
    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        return NULL;
    }

    const size_t active_total = download_active_list(NULL, 0U);
    DownloadActiveInfo *active_items = NULL;
    if (active_total > 0U) {
        active_items = calloc(active_total, sizeof(DownloadActiveInfo));
        if (active_items != NULL) {
            (void)download_active_list(active_items, active_total);
        }
    }

    cJSON *downloads = cJSON_CreateArray();
    if (downloads != NULL) {
        if (active_items != NULL) {
            for (size_t i = 0; i < active_total; ++i) {
                const DownloadActiveInfo *item = &active_items[i];
                cJSON *entry = cJSON_CreateObject();
                if (entry == NULL) {
                    continue;
                }
                cJSON_AddStringToObject(entry, AVAR_FIELD_ID, item->id);
                cJSON_AddStringToObject(entry, AVAR_FIELD_FILENAME, item->filename);
                cJSON_AddNumberToObject(entry, AVAR_FIELD_BYTES_DOWNLOADED,
                                        (double)item->bytes_downloaded);
                cJSON_AddNumberToObject(entry, AVAR_FIELD_TOTAL_BYTES, (double)item->total_bytes);
                cJSON_AddItemToArray(downloads, entry);
            }
        }
        cJSON_AddItemToObject(result, "downloads", downloads);
    }
    free(active_items);

    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddNumberToObject(result, "queueCount", (double)queue_count());
    cJSON_AddNumberToObject(result, "activeDownloads", (double)download_active_count());
    cJSON_AddNumberToObject(result, "uptimeSeconds",
                            (double)(_daemon_started_at > 0 ? time(NULL) - _daemon_started_at : 0));
    cJSON_AddBoolToObject(result, "fileDownloadEnabled", daemon_server_file_download_enabled());
    cJSON_AddBoolToObject(result, "fsBrowseEnabled", daemon_server_fs_browse_enabled());
    return result;
}

static uint64_t sum_download_bytes(void) {
    uint64_t total = 0U;
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *done_str = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_BYTES_DOWNLOADED);
        if (done_str != NULL) {
            total += strtoull(done_str, NULL, 10);
            free(done_str);
        }
    }
    return total;
}

static cJSON *handle_system_stats(void) {
    SystemStats stats;
    if (system_stats_collect(&stats) != 0) {
        return NULL;
    }

    static uint64_t last_rx_bytes = 0U;
    static time_t last_sample_at = 0;
    const uint64_t rx_total = sum_download_bytes();
    const time_t now = time(NULL);
    uint64_t rx_per_sec = 0U;
    if (last_sample_at > 0 && now > last_sample_at && rx_total >= last_rx_bytes) {
        rx_per_sec = (rx_total - last_rx_bytes) / (uint64_t)(now - last_sample_at);
    }
    last_rx_bytes = rx_total;
    last_sample_at = now;
    stats.network_rx_bytes_per_sec = rx_per_sec;

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddNumberToObject(result, "diskTotalBytes", (double)stats.disk_total_bytes);
    cJSON_AddNumberToObject(result, "diskFreeBytes", (double)stats.disk_free_bytes);
    cJSON_AddNumberToObject(result, "memoryTotalBytes", (double)stats.memory_total_bytes);
    cJSON_AddNumberToObject(result, "memoryUsedBytes", (double)stats.memory_used_bytes);
    cJSON_AddNumberToObject(result, "memoryUsedPercent", stats.memory_used_percent);
    cJSON_AddNumberToObject(result, "cpuUsagePercent", stats.cpu_usage_percent);
    cJSON_AddNumberToObject(result, "networkRxBytesPerSec", (double)stats.network_rx_bytes_per_sec);
    cJSON_AddNumberToObject(result, "networkTxBytesPerSec", (double)stats.network_tx_bytes_per_sec);
    return result;
}

static cJSON *handle_download_add(cJSON *params) {
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(params, "url");
    if (!cJSON_IsString(url) || url->valuestring == NULL) {
        return NULL;
    }

    char *normalized_url = strdup(url->valuestring);
    if (normalized_url == NULL) {
        return NULL;
    }
    trim_whitespace_inplace(normalized_url);

    const cJSON *queue = cJSON_GetObjectItemCaseSensitive(params, "queue");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, "name");

    const char *queue_name =
        cJSON_IsString(queue) && queue->valuestring != NULL ? queue->valuestring : NULL;
    const char *dl_name = cJSON_IsString(name) && name->valuestring != NULL ? name->valuestring : NULL;

    const cJSON *proxy = cJSON_GetObjectItemCaseSensitive(params, "proxy");
    char *proxy_url = proxy_url_from_json(proxy);

    const cJSON *stream_kind_json = cJSON_GetObjectItemCaseSensitive(params, "streamKind");
    const cJSON *referer_json = cJSON_GetObjectItemCaseSensitive(params, "referer");
    const char *stream_kind =
        cJSON_IsString(stream_kind_json) && stream_kind_json->valuestring != NULL
            ? stream_kind_json->valuestring
            : NULL;
    const char *referer =
        cJSON_IsString(referer_json) && referer_json->valuestring != NULL
            ? referer_json->valuestring
            : NULL;

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        free(normalized_url);
        free(proxy_url);
        return NULL;
    }

    if (!is_valid_http_url(normalized_url)) {
        cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        free(normalized_url);
        free(proxy_url);
        return result;
    }

    const cJSON *start_now = cJSON_GetObjectItemCaseSensitive(params, "startNow");
    const bool should_start = cJSON_IsTrue(start_now);
    const cJSON *force_new = cJSON_GetObjectItemCaseSensitive(params, "forceNew");
    const bool force_new_id = cJSON_IsTrue(force_new);

    char *id = NULL;
    int rc = download_enqueue_ex(normalized_url, queue_name, dl_name, proxy_url, stream_kind,
                                 referer, force_new_id, &id);
    if (rc == EXIT_SUCCESS && should_start && id != NULL) {
        rc = download_start(id);
    }
    free(normalized_url);
    free(proxy_url);
    cJSON_AddNumberToObject(result, "exitCode", rc);
    if (id != NULL) {
        cJSON_AddStringToObject(result, "id", id);
        free(id);
    }
    return result;
}

static cJSON *handle_cli_exec(cJSON *params, char **stdout_out) {
    const cJSON *argv_json = cJSON_GetObjectItemCaseSensitive(params, "argv");
    if (!cJSON_IsArray(argv_json)) {
        return NULL;
    }

    const int argc = cJSON_GetArraySize(argv_json);
    if (argc <= 0) {
        return NULL;
    }

    char **argv = calloc((size_t)argc + 1U, sizeof(char *));
    if (argv == NULL) {
        return NULL;
    }

    for (int i = 0; i < argc; ++i) {
        const cJSON *item = cJSON_GetArrayItem(argv_json, i);
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            for (int j = 0; j < i; ++j) {
                free(argv[j]);
            }
            free(argv);
            return NULL;
        }
        argv[i] = strdup(item->valuestring);
    }

    daemon_session_force_local(true);
    char *output = NULL;
    const int rc = dispatch_cli_command(argc, argv, &output);
    daemon_session_force_local(false);

    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    free(argv);

    if (stdout_out != NULL) {
        *stdout_out = output;
    } else {
        free(output);
    }

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", rc);
        if (output != NULL) {
            cJSON_AddStringToObject(result, "output", output);
        }
    }
    return result;
}

static cJSON *handle_logs_get(cJSON *params) {
    const cJSON *max_lines = cJSON_GetObjectItemCaseSensitive(params, "maxLines");
    const cJSON *follow = cJSON_GetObjectItemCaseSensitive(params, "follow");
    const cJSON *since_json = cJSON_GetObjectItemCaseSensitive(params, "since");
    unsigned limit = 50U;
    if (cJSON_IsNumber(max_lines) && max_lines->valuedouble > 0) {
        limit = (unsigned)max_lines->valuedouble;
    }

    uint64_t since = 0U;
    if (cJSON_IsNumber(since_json) && since_json->valuedouble >= 0) {
        since = (uint64_t)since_json->valuedouble;
    }

    char *text = NULL;
    uint64_t next_offset = since;
    if (!daemon_rpc_logs_fetch(cJSON_IsTrue(follow), limit, since, &text, &next_offset)) {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddStringToObject(result, "logs", text != NULL ? text : "");
        cJSON_AddNumberToObject(result, "nextOffset", (double)next_offset);
    }
    free(text);
    return result;
}

static int queue_error_exit_code(const QueueError error) {
    return error == QueueErrorNone ? EXIT_SUCCESS : EXIT_FAILURE;
}

static cJSON *queue_object_from_index(const size_t index) {
    char *json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, index);
    if (json == NULL) {
        return NULL;
    }
    cJSON *obj = cJSON_Parse(json);
    free(json);
    return obj;
}

static cJSON *handle_queue_list(void) {
    cJSON *result = cJSON_CreateObject();
    cJSON *queues = cJSON_CreateArray();
    if (result == NULL || queues == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(queues);
        return NULL;
    }

    const size_t count = queue_count();
    for (size_t i = 0; i < count; ++i) {
        cJSON *entry = queue_object_from_index(i);
        if (entry != NULL) {
            cJSON_AddItemToArray(queues, entry);
        }
    }

    cJSON_AddNumberToObject(result, "exitCode", EXIT_SUCCESS);
    cJSON_AddItemToObject(result, "queues", queues);
    return result;
}

static void enrich_download_entry_dest_path(cJSON *entry) {
    if (entry == NULL) {
        return;
    }

    const cJSON *status = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_STATUS);
    const cJSON *existing = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_DEST_PATH);
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_ID);
    if (!cJSON_IsString(status) || strcmp(status->valuestring, AVAR_DL_STATUS_COMPLETED) != 0) {
        return;
    }
    if (cJSON_IsString(existing) && existing->valuestring != NULL && existing->valuestring[0] != '\0') {
        return;
    }
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0') {
        return;
    }

    char *path = NULL;
    if (download_resolve_dest_path(id->valuestring, &path) != EXIT_SUCCESS || path == NULL) {
        return;
    }

    if (cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_DEST_PATH) != NULL) {
        cJSON_ReplaceItemInObjectCaseSensitive(entry, AVAR_FIELD_DEST_PATH,
                                               cJSON_CreateString(path));
    } else {
        cJSON_AddStringToObject(entry, AVAR_FIELD_DEST_PATH, path);
    }
    free(path);
}

static cJSON *handle_downloads_list(void) {
    cJSON *result = cJSON_CreateObject();
    cJSON *downloads = cJSON_CreateArray();
    if (result == NULL || downloads == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(downloads);
        return NULL;
    }

    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, i);
        if (json == NULL) {
            continue;
        }
        cJSON *entry = cJSON_Parse(json);
        free(json);
        if (entry != NULL) {
            enrich_download_entry_dest_path(entry);
            cJSON_AddItemToArray(downloads, entry);
        }
    }

    cJSON_AddNumberToObject(result, "exitCode", EXIT_SUCCESS);
    cJSON_AddItemToObject(result, "downloads", downloads);
    return result;
}

static cJSON *handle_fs_browse(cJSON *params) {
    const cJSON *path_node = cJSON_GetObjectItemCaseSensitive(params, "path");
    const char *requested =
        cJSON_IsString(path_node) && path_node->valuestring != NULL ? path_node->valuestring : "";

    AvarDirListing listing = {0};
    if (list_directory_entries(requested, &listing) != 0) {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *entries = cJSON_CreateArray();
    if (result == NULL || entries == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(entries);
        avar_dir_listing_free(&listing);
        return NULL;
    }

    if (listing.path != NULL) {
        cJSON_AddStringToObject(result, "path", listing.path);
    }
    if (listing.parent != NULL) {
        cJSON_AddStringToObject(result, "parent", listing.parent);
    }

    for (size_t i = 0; i < listing.count; ++i) {
        if (!listing.entries[i].is_dir || listing.entries[i].name == NULL) {
            continue;
        }
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            continue;
        }
        cJSON_AddStringToObject(entry, "name", listing.entries[i].name);
        cJSON_AddItemToArray(entries, entry);
    }

    cJSON_AddItemToObject(result, "entries", entries);
    avar_dir_listing_free(&listing);
    return result;
}

static QueueOptions parse_queue_options(cJSON *params) {
    QueueOptions options = {0};
    if (params == NULL) {
        return options;
    }

    const cJSON *description = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_DESCRIPTION);
    if (cJSON_IsString(description) && description->valuestring != NULL) {
        options.description = description->valuestring;
    }

    const cJSON *max_concurrent =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_MAX_CONCURRENT);
    if (cJSON_IsNumber(max_concurrent) && max_concurrent->valuedouble > 0) {
        options.max_concurrent_downloads = (uint32_t)max_concurrent->valuedouble;
    }

    const cJSON *max_connections =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_MAX_CONNECTIONS);
    if (cJSON_IsNumber(max_connections) && max_connections->valuedouble > 0) {
        options.max_connections = (uint32_t)max_connections->valuedouble;
    }

    const cJSON *temp_path = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_TEMP_PATH);
    if (cJSON_IsString(temp_path) && temp_path->valuestring != NULL) {
        options.temp_path = temp_path->valuestring;
    }

    const cJSON *download_path =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_DOWNLOAD_PATH);
    if (cJSON_IsString(download_path) && download_path->valuestring != NULL) {
        options.download_path = download_path->valuestring;
    }

    return options;
}

static QueuePatch parse_queue_patch(cJSON *params) {
    QueuePatch patch = {0};
    if (params == NULL) {
        return patch;
    }

    const cJSON *description = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_DESCRIPTION);
    if (cJSON_IsString(description)) {
        patch.set_description = true;
        patch.description = description->valuestring;
    }

    const cJSON *max_concurrent =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_MAX_CONCURRENT);
    if (cJSON_IsNumber(max_concurrent)) {
        patch.set_max_concurrent_downloads = true;
        patch.max_concurrent_downloads = (uint32_t)max_concurrent->valuedouble;
    }

    const cJSON *max_connections =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_MAX_CONNECTIONS);
    if (cJSON_IsNumber(max_connections)) {
        patch.set_max_connections = true;
        patch.max_connections = (uint32_t)max_connections->valuedouble;
    }

    const cJSON *temp_path = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_TEMP_PATH);
    if (cJSON_IsString(temp_path)) {
        patch.set_temp_path = true;
        patch.temp_path = temp_path->valuestring;
    }

    const cJSON *download_path =
        cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_DOWNLOAD_PATH);
    if (cJSON_IsString(download_path)) {
        patch.set_download_path = true;
        patch.download_path = download_path->valuestring;
    }

    return patch;
}

static cJSON *handle_queue_add(cJSON *params) {
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_NAME);
    if (!cJSON_IsString(name) || name->valuestring == NULL) {
        return NULL;
    }

    const QueueOptions options = parse_queue_options(params);
    char *id = NULL;
    const QueueError rc = queue_add(name->valuestring, &options, &id);

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        free(id);
        return NULL;
    }

    cJSON_AddNumberToObject(result, "exitCode", queue_error_exit_code(rc));
    if (id != NULL) {
        cJSON_AddStringToObject(result, "id", id);
        free(id);
    }
    return result;
}

static cJSON *handle_queue_remove(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_NAME);
    const cJSON *purge = cJSON_GetObjectItemCaseSensitive(params, "purgeItems");
    const bool by_name = cJSON_IsString(name) && name->valuestring != NULL;
    const char *target = by_name ? name->valuestring
                                 : (cJSON_IsString(id) && id->valuestring != NULL ? id->valuestring
                                                                                  : NULL);
    if (target == NULL) {
        return NULL;
    }

    const QueueError rc =
        queue_remove(target, by_name, cJSON_IsTrue(purge));

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", queue_error_exit_code(rc));
    }
    return result;
}

static cJSON *handle_queue_edit(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    if (!cJSON_IsString(id) || id->valuestring == NULL) {
        return NULL;
    }

    const QueuePatch patch = parse_queue_patch(params);
    const QueueError rc = queue_edit(id->valuestring, &patch);

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", queue_error_exit_code(rc));
    }
    return result;
}

static cJSON *handle_download_watch(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0') {
        return NULL;
    }

    download_progress_watch(id->valuestring);

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", EXIT_SUCCESS);
    }
    return result;
}

static cJSON *handle_download_unwatch(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0') {
        return NULL;
    }

    download_progress_unwatch(id->valuestring);

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", EXIT_SUCCESS);
    }
    return result;
}

static cJSON *handle_download_checksum(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    const cJSON *algorithm = cJSON_GetObjectItemCaseSensitive(params, "algorithm");
    const cJSON *expected = cJSON_GetObjectItemCaseSensitive(params, "expected");
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0' ||
        !cJSON_IsString(algorithm) || algorithm->valuestring == NULL ||
        algorithm->valuestring[0] == '\0') {
        return NULL;
    }

    if (!file_checksum_algorithm_supported(algorithm->valuestring)) {
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        }
        return result;
    }

    char *path = NULL;
    if (download_resolve_dest_path(id->valuestring, &path) != EXIT_SUCCESS || path == NULL) {
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        }
        return result;
    }

    char *checksum = NULL;
    const int rc = file_checksum_file(path, algorithm->valuestring, &checksum);
    free(path);

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        free(checksum);
        return NULL;
    }

    cJSON_AddNumberToObject(result, "exitCode", rc);
    if (rc == EXIT_SUCCESS && checksum != NULL) {
        cJSON_AddStringToObject(result, "checksum", checksum);
        if (cJSON_IsString(expected) && expected->valuestring != NULL &&
            expected->valuestring[0] != '\0') {
            cJSON_AddBoolToObject(result, "match",
                                  file_checksum_matches(checksum, expected->valuestring));
        }
    }
    free(checksum);
    return result;
}

static cJSON *handle_download_resolve_path(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0') {
        return NULL;
    }

    char *path = NULL;
    if (download_resolve_dest_path(id->valuestring, &path) != EXIT_SUCCESS || path == NULL) {
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        }
        return result;
    }

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        free(path);
        return NULL;
    }

    cJSON_AddNumberToObject(result, "exitCode", EXIT_SUCCESS);
    cJSON_AddStringToObject(result, AVAR_FIELD_DEST_PATH, path);
    free(path);
    return result;
}

static cJSON *handle_queue_start(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_NAME);
    const bool by_name = cJSON_IsString(name) && name->valuestring != NULL;
    const char *target = by_name ? name->valuestring
                                 : (cJSON_IsString(id) && id->valuestring != NULL ? id->valuestring
                                                                                  : NULL);
    if (target == NULL) {
        return NULL;
    }

    char *resolved = queue_resolve_id(target, by_name);
    if (resolved == NULL) {
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        }
        return result;
    }

    const QueueError rc = queue_start(resolved);
    free(resolved);

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", queue_error_exit_code(rc));
    }
    return result;
}

static cJSON *handle_queue_stop(cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, AVAR_FIELD_ID);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, AVAR_QUEUE_FIELD_NAME);
    const bool by_name = cJSON_IsString(name) && name->valuestring != NULL;
    const char *target = by_name ? name->valuestring
                                 : (cJSON_IsString(id) && id->valuestring != NULL ? id->valuestring
                                                                                  : NULL);
    if (target == NULL) {
        return NULL;
    }

    char *resolved = queue_resolve_id(target, by_name);
    if (resolved == NULL) {
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", EXIT_FAILURE);
        }
        return result;
    }

    const QueueError rc = queue_stop(resolved);
    free(resolved);

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddNumberToObject(result, "exitCode", queue_error_exit_code(rc));
    }
    return result;
}

#define AVAR_STREAM_KIND_NONE 0
#define AVAR_STREAM_KIND_SSE 1
#define AVAR_STREAM_KIND_WS 2

typedef struct StreamClient {
    struct mg_connection *connection;
    bool websocket;
    struct StreamClient *next;
} StreamClient;

#define AVAR_STREAM_FULL_RESYNC_SEC 60
#define AVAR_STREAM_UNCHANGED_JSON "{\"type\":\"unchanged\"}"
#define AVAR_FNV1A_OFFSET 14695981039346656037ULL
#define AVAR_FNV1A_PRIME 1099511628211ULL

static StreamClient *_stream_clients = NULL;

static void stream_client_unregister(struct mg_connection *connection);

static uint64_t fnv1a_update(uint64_t hash, const unsigned char *data, const size_t len) {
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= AVAR_FNV1A_PRIME;
    }
    return hash;
}

static uint64_t stream_content_fingerprint(void) {
    uint64_t fp = AVAR_FNV1A_OFFSET;

    const size_t queue_total = queue_count();
    fp ^= (uint64_t)queue_total;
    fp *= AVAR_FNV1A_PRIME;
    for (size_t i = 0; i < queue_total; ++i) {
        char *json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, i);
        if (json != NULL) {
            fp = fnv1a_update(fp, (const unsigned char *)json, strlen(json));
            free(json);
        }
    }

    const size_t download_total = get_config_array_size(AVAR_CFG_DM_ITEMS);
    fp ^= (uint64_t)download_total;
    fp *= AVAR_FNV1A_PRIME;
    for (size_t i = 0; i < download_total; ++i) {
        char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, i);
        if (json != NULL) {
            fp = fnv1a_update(fp, (const unsigned char *)json, strlen(json));
            free(json);
        }
    }

    const size_t active_total = download_active_list(NULL, 0U);
    fp ^= (uint64_t)active_total;
    fp *= AVAR_FNV1A_PRIME;
    if (active_total > 0U) {
        DownloadActiveInfo *active_items = calloc(active_total, sizeof(DownloadActiveInfo));
        if (active_items != NULL) {
            (void)download_active_list(active_items, active_total);
            for (size_t i = 0; i < active_total; ++i) {
                const DownloadActiveInfo *item = &active_items[i];
                if (item->id != NULL) {
                    fp = fnv1a_update(fp, (const unsigned char *)item->id, strlen(item->id));
                }
                const uint64_t bytes_downloaded = item->bytes_downloaded;
                const uint64_t total_bytes = item->total_bytes;
                fp = fnv1a_update(fp, (const unsigned char *)&bytes_downloaded,
                                  sizeof bytes_downloaded);
                fp = fnv1a_update(fp, (const unsigned char *)&total_bytes, sizeof total_bytes);
            }
            free(active_items);
        }
    }

    return fp;
}

static void stream_note_full_snapshot_sent(void) {
    _stream_cached_fingerprint = stream_content_fingerprint();
    _stream_last_full_send = time(NULL);
}

static void stream_send_to_connection(struct mg_connection *connection, const bool websocket,
                                      const char *payload, const bool snapshot_event) {
    if (connection == NULL || payload == NULL) {
        return;
    }

    if (websocket) {
        mg_ws_send(connection, payload, strlen(payload), WEBSOCKET_OP_TEXT);
        return;
    }

    if (snapshot_event) {
        mg_printf(connection, "event: snapshot\ndata: %s\n\n", payload);
    } else {
        mg_printf(connection, "event: unchanged\ndata: %s\n\n", payload);
    }
}

static void stream_broadcast_payload(const char *payload, const bool snapshot_event) {
    StreamClient *cursor = _stream_clients;
    while (cursor != NULL) {
        StreamClient *next = cursor->next;
        if (cursor->connection == NULL || cursor->connection->is_closing) {
            stream_client_unregister(cursor->connection);
        } else {
            stream_send_to_connection(cursor->connection, cursor->websocket, payload,
                                      snapshot_event);
        }
        cursor = next;
    }
}

static size_t stream_client_count(void) {
    size_t count = 0U;
    for (StreamClient *cursor = _stream_clients; cursor != NULL; cursor = cursor->next) {
        if (cursor->connection != NULL && !cursor->connection->is_closing) {
            count++;
        }
    }
    return count;
}

void daemon_rpc_note_frontend_activity(void) {
    _frontend_last_activity = time(NULL);
}

bool daemon_rpc_frontend_clients_active(const unsigned grace_seconds) {
    if (stream_client_count() > 0U) {
        return true;
    }
    if (_frontend_last_activity == 0) {
        return false;
    }
    const time_t now = time(NULL);
    return now - _frontend_last_activity < (time_t)grace_seconds;
}

static void stream_client_register(struct mg_connection *connection, const bool websocket) {
    if (connection == NULL) {
        return;
    }

    connection->data[0] = (char)(websocket ? AVAR_STREAM_KIND_WS : AVAR_STREAM_KIND_SSE);

    StreamClient *node = calloc(1U, sizeof(StreamClient));
    if (node == NULL) {
        return;
    }
    node->connection = connection;
    node->websocket = websocket;
    node->next = _stream_clients;
    _stream_clients = node;
    daemon_rpc_note_frontend_activity();
}

static void stream_client_unregister(struct mg_connection *connection) {
    if (connection == NULL) {
        return;
    }

    connection->data[0] = (char)AVAR_STREAM_KIND_NONE;

    StreamClient **cursor = &_stream_clients;
    while (*cursor != NULL) {
        if ((*cursor)->connection == connection) {
            StreamClient *removed = *cursor;
            *cursor = removed->next;
            free(removed);
            return;
        }
        cursor = &(*cursor)->next;
    }
}

bool daemon_rpc_build_snapshot(char **json_out) {
    if (json_out == NULL) {
        return false;
    }
    *json_out = NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *health = handle_health();
    cJSON *queues = cJSON_CreateArray();
    cJSON *downloads = cJSON_CreateArray();
    if (root == NULL || health == NULL || queues == NULL || downloads == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(health);
        cJSON_Delete(queues);
        cJSON_Delete(downloads);
        return false;
    }

    const size_t queue_total = queue_count();
    for (size_t i = 0; i < queue_total; ++i) {
        cJSON *entry = queue_object_from_index(i);
        if (entry != NULL) {
            cJSON_AddItemToArray(queues, entry);
        }
    }

    const size_t download_total = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < download_total; ++i) {
        char *item_json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, i);
        if (item_json == NULL) {
            continue;
        }
        cJSON *entry = cJSON_Parse(item_json);
        free(item_json);
        if (entry != NULL) {
            enrich_download_entry_dest_path(entry);
            cJSON_AddItemToArray(downloads, entry);
        }
    }

    cJSON *stats = handle_system_stats();

    cJSON_AddStringToObject(root, "type", "snapshot");
    cJSON_AddItemToObject(root, "health", health);
    cJSON_AddItemToObject(root, "queues", queues);
    cJSON_AddItemToObject(root, "downloads", downloads);
    if (stats != NULL) {
        cJSON_AddItemToObject(root, "stats", stats);
    }

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == NULL) {
        return false;
    }

    *json_out = printed;
    return true;
}

void daemon_rpc_stream_send(struct mg_connection *connection, const bool websocket) {
    if (connection == NULL) {
        return;
    }

    char *snapshot = NULL;
    if (!daemon_rpc_build_snapshot(&snapshot) || snapshot == NULL) {
        return;
    }

    stream_send_to_connection(connection, websocket, snapshot, true);
    stream_note_full_snapshot_sent();
    free(snapshot);
}

void daemon_rpc_streams_tick(struct mg_mgr *mgr) {
    (void)mgr;

    const time_t now = time(NULL);
    if (_stream_clients == NULL) {
        return;
    }
    if (_stream_last_tick != 0 && now - _stream_last_tick < 1) {
        return;
    }
    _stream_last_tick = now;

    const bool force_full =
        _stream_last_full_send == 0 ||
        now - _stream_last_full_send >= (time_t)AVAR_STREAM_FULL_RESYNC_SEC;
    const uint64_t fingerprint = stream_content_fingerprint();
    const bool content_changed = fingerprint != _stream_cached_fingerprint;

    if (!force_full && !content_changed) {
        stream_broadcast_payload(AVAR_STREAM_UNCHANGED_JSON, false);
        daemon_rpc_note_frontend_activity();
        return;
    }

    char *snapshot = NULL;
    if (!daemon_rpc_build_snapshot(&snapshot) || snapshot == NULL) {
        return;
    }

    stream_broadcast_payload(snapshot, true);
    _stream_cached_fingerprint = fingerprint;
    _stream_last_full_send = now;
    free(snapshot);
}

void daemon_rpc_stream_attach_sse(struct mg_connection *connection) {
    stream_client_register(connection, false);
    daemon_rpc_stream_send(connection, false);
}

void daemon_rpc_stream_attach_ws(struct mg_connection *connection) {
    stream_client_register(connection, true);
    daemon_rpc_stream_send(connection, true);
}

void daemon_rpc_stream_detach(struct mg_connection *connection) {
    stream_client_unregister(connection);
}

void daemon_rpc_stream_handle_ws_message(struct mg_connection *connection, const char *payload,
                                         const size_t len) {
    if (connection == NULL || payload == NULL || len < 4U ||
        memcmp(payload, "ping", 4) != 0) {
        return;
    }

    daemon_rpc_note_frontend_activity();

    cJSON *stats = handle_system_stats();
    if (stats == NULL) {
        return;
    }
    cJSON_AddStringToObject(stats, "type", "stats");

    char *printed = cJSON_PrintUnformatted(stats);
    cJSON_Delete(stats);
    if (printed == NULL) {
        return;
    }

    mg_ws_send(connection, printed, strlen(printed), WEBSOCKET_OP_TEXT);
    free(printed);
}

static cJSON *dispatch_method(const char *method, cJSON *params, cJSON *id) {
    (void)id;

    if (method == NULL) {
        return NULL;
    }

    if (strcmp(method, "ping") == 0) {
        return handle_ping();
    }
    if (strcmp(method, "health") == 0) {
        return handle_health();
    }
    if (strcmp(method, "system.stats") == 0) {
        return handle_system_stats();
    }
    if (strcmp(method, "download.add") == 0) {
        return handle_download_add(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "cli.exec") == 0) {
        return handle_cli_exec(params != NULL ? params : cJSON_CreateObject(), NULL);
    }
    if (strcmp(method, "logs.get") == 0) {
        return handle_logs_get(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "queue.list") == 0) {
        return handle_queue_list();
    }
    if (strcmp(method, "queue.add") == 0) {
        return handle_queue_add(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "queue.remove") == 0) {
        return handle_queue_remove(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "queue.edit") == 0) {
        return handle_queue_edit(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "queue.start") == 0) {
        return handle_queue_start(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "queue.stop") == 0) {
        return handle_queue_stop(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "downloads.list") == 0) {
        return handle_downloads_list();
    }
    if (strcmp(method, "download.watch") == 0) {
        return handle_download_watch(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "download.unwatch") == 0) {
        return handle_download_unwatch(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "download.checksum") == 0) {
        return handle_download_checksum(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "download.resolvePath") == 0) {
        return handle_download_resolve_path(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "fs.browse") == 0) {
        if (!daemon_server_fs_browse_enabled()) {
            return NULL;
        }
        return handle_fs_browse(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "daemon.reload") == 0) {
        DaemonConfig cfg;
        daemon_config_load(&cfg);
        cJSON *result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddNumberToObject(result, "exitCode", daemon_reload_config(&cfg));
        }
        return result;
    }

    return NULL;
}

static bool daemon_rpc_handle_unlocked(const char *request_json, char **response_json_out) {
    if (response_json_out != NULL) {
        *response_json_out = NULL;
    }
    if (request_json == NULL || response_json_out == NULL) {
        return false;
    }

    cJSON *req = cJSON_Parse(request_json);
    if (req == NULL) {
        return false;
    }

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(req, "jsonrpc");
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(req, "method");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(req, "params");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");

    cJSON *response = NULL;
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method)) {
        response = rpc_error(-32600, "Invalid Request", id);
    } else {
        cJSON *result = dispatch_method(method->valuestring, params, id);
        if (result == NULL) {
            response = rpc_error(-32601, "Method not found", id);
        } else {
            response = rpc_result(result, id);
        }
    }

    char *printed = cJSON_PrintUnformatted(response);
    cJSON_Delete(req);
    cJSON_Delete(response);
    if (printed == NULL) {
        return false;
    }

    *response_json_out = printed;
    return true;
}

bool daemon_rpc_handle(const char *request_json, char **response_json_out) {
    rpc_lock();
    const bool ok = daemon_rpc_handle_unlocked(request_json, response_json_out);
    rpc_unlock();
    return ok;
}

static bool daemon_rpc_handle_http_unlocked(const char *uri, const char *body, const size_t body_len,
                                            char **body_out, int *status_out) {
    if (body_out != NULL) {
        *body_out = NULL;
    }
    if (status_out != NULL) {
        *status_out = 500;
    }

    if (uri == NULL || body_out == NULL || status_out == NULL) {
        return false;
    }

    daemon_rpc_note_frontend_activity();

    if (strcmp(uri, "/api/health") == 0) {
        cJSON *health = handle_health();
        char *printed = cJSON_PrintUnformatted(health);
        cJSON_Delete(health);
        if (printed == NULL) {
            return false;
        }
        *body_out = printed;
        *status_out = 200;
        return true;
    }

    if (strcmp(uri, "/api/stats") == 0) {
        cJSON *stats = handle_system_stats();
        char *printed = stats != NULL ? cJSON_PrintUnformatted(stats) : NULL;
        cJSON_Delete(stats);
        if (printed == NULL) {
            return false;
        }
        *body_out = printed;
        *status_out = 200;
        return true;
    }

    if (strcmp(uri, "/api/rpc") == 0) {
        char request_buf[AVAR_DAEMON_RPC_FRAME_MAX];
        if (body == NULL || body_len == 0U) {
            *body_out = strdup("{\"error\":\"empty body\"}\n");
            *status_out = 400;
            return *body_out != NULL;
        }
        if (body_len >= sizeof request_buf) {
            *body_out = strdup("{\"error\":\"request too large\"}\n");
            *status_out = 413;
            return *body_out != NULL;
        }
        memcpy(request_buf, body, body_len);
        request_buf[body_len] = '\0';

        char *rpc_response = NULL;
        if (!daemon_rpc_handle_unlocked(request_buf, &rpc_response)) {
            *body_out = strdup("{\"error\":\"invalid rpc\"}\n");
            *status_out = 400;
            return *body_out != NULL;
        }
        *body_out = rpc_response;
        *status_out = 200;
        return true;
    }

    *body_out = strdup("{\"error\":\"unsupported\"}\n");
    *status_out = 404;
    return *body_out != NULL;
}

bool daemon_rpc_handle_http(const char *uri, const char *body, const size_t body_len,
                            char **body_out, int *status_out) {
    rpc_lock();
    const bool ok = daemon_rpc_handle_http_unlocked(uri, body, body_len, body_out, status_out);
    rpc_unlock();
    return ok;
}

bool daemon_rpc_call(const AvarTransportKind transport, const DaemonConfig *cfg,
                     const char *method, const char *params_json, char **result_json_out,
                     int *exit_code_out) {
    if (cfg == NULL || method == NULL) {
        return false;
    }

    cJSON *req = cJSON_CreateObject();
    if (req == NULL) {
        return false;
    }
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", method);
    cJSON_AddNumberToObject(req, "id", 1);

    if (params_json != NULL && params_json[0] != '\0') {
        cJSON *params = cJSON_Parse(params_json);
        if (params == NULL) {
            cJSON_Delete(req);
            return false;
        }
        cJSON_AddItemToObject(req, "params", params);
    } else {
        cJSON_AddObjectToObject(req, "params");
    }

    char *request = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (request == NULL) {
        return false;
    }

    char *response = NULL;
    const bool ok = daemon_transport_rpc_request_any(cfg, request, &response);
    (void)transport;
    free(request);
    if (!ok || response == NULL) {
        free(response);
        return false;
    }

    cJSON *parsed = cJSON_Parse(response);
    free(response);
    if (parsed == NULL) {
        return false;
    }

    const cJSON *error = cJSON_GetObjectItemCaseSensitive(parsed, "error");
    if (cJSON_IsObject(error)) {
        cJSON_Delete(parsed);
        return false;
    }

    const cJSON *result = cJSON_GetObjectItemCaseSensitive(parsed, "result");
    if (result_json_out != NULL && result != NULL) {
        *result_json_out = cJSON_PrintUnformatted(result);
    }

    if (exit_code_out != NULL && cJSON_IsObject(result)) {
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(result, "exitCode");
        if (cJSON_IsNumber(code)) {
            *exit_code_out = (int)code->valuedouble;
        }
    }

    cJSON_Delete(parsed);
    return true;
}

int daemon_rpc_delegate_argv(const int argc, char **argv) {
    if (argc <= 0 || argv == NULL) {
        return EXIT_FAILURE;
    }

    cJSON *params = cJSON_CreateObject();
    cJSON *argv_json = cJSON_CreateArray();
    if (params == NULL || argv_json == NULL) {
        cJSON_Delete(params);
        cJSON_Delete(argv_json);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < argc; ++i) {
        cJSON_AddItemToArray(argv_json, cJSON_CreateString(argv[i]));
    }
    cJSON_AddItemToObject(params, "argv", argv_json);

    char *params_str = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (params_str == NULL) {
        return EXIT_FAILURE;
    }

    const DaemonConfig *cfg = daemon_session_config();
    if (cfg == NULL) {
        free(params_str);
        return EXIT_FAILURE;
    }

    char *result_json = NULL;
    int exit_code = EXIT_FAILURE;
    const bool ok = daemon_rpc_call(cfg->session.transport, cfg, "cli.exec", params_str,
                                    &result_json, &exit_code);
    free(params_str);

    if (!ok) {
        free(result_json);
        LOG_ERROR("Failed to delegate command to daemon");
        return EXIT_FAILURE;
    }

    if (result_json != NULL) {
        cJSON *result = cJSON_Parse(result_json);
        free(result_json);
        if (result != NULL) {
            const cJSON *output = cJSON_GetObjectItemCaseSensitive(result, "output");
            if (cJSON_IsString(output) && output->valuestring != NULL) {
                fputs(output->valuestring, stdout);
            }
            const cJSON *code = cJSON_GetObjectItemCaseSensitive(result, "exitCode");
            if (cJSON_IsNumber(code)) {
                exit_code = (int)code->valuedouble;
            }
            cJSON_Delete(result);
        }
    }

    return exit_code;
}
