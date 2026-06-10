#include <cJSON.h>

#include <cli.h>
#include <daemon.h>
#include <daemon_rpc.h>
#include <daemon_session.h>
#include <daemon_transport.h>
#include <download.h>
#include <logger.h>
#include <mongoose.h>
#include <queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <io.h>
    #include <fcntl.h>
#else
    #include <unistd.h>
#endif

#define AVAR_DAEMON_LOG_RING_CAP 256U
#define AVAR_DAEMON_LOG_LINE_MAX 512U

typedef struct {
    char lines[AVAR_DAEMON_LOG_RING_CAP][AVAR_DAEMON_LOG_LINE_MAX];
    size_t head;
    size_t count;
} DaemonLogRing;

static DaemonLogRing _log_ring = {0};
static char _auth_token[128] = {0};
static time_t _daemon_started_at = 0;

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
    _daemon_started_at = time(NULL);
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
    (void)download_wait_idle(30000U);
}

void daemon_rpc_log_append(const char *line) {
    if (line == NULL) {
        return;
    }

    snprintf(_log_ring.lines[_log_ring.head], AVAR_DAEMON_LOG_LINE_MAX, "%s", line);
    _log_ring.head = (_log_ring.head + 1U) % AVAR_DAEMON_LOG_RING_CAP;
    if (_log_ring.count < AVAR_DAEMON_LOG_RING_CAP) {
        _log_ring.count++;
    }
}

bool daemon_rpc_logs_fetch(const bool follow, const unsigned max_lines, char **text_out) {
    (void)follow;

    if (text_out == NULL) {
        return false;
    }

    const unsigned limit = max_lines == 0U ? 50U : max_lines;
    size_t total = 0;
    for (size_t i = 0; i < _log_ring.count && i < limit; ++i) {
        const size_t idx =
            (_log_ring.head + AVAR_DAEMON_LOG_RING_CAP - _log_ring.count + i) % AVAR_DAEMON_LOG_RING_CAP;
        total += strlen(_log_ring.lines[idx]) + 1U;
    }

    char *buf = calloc(total + 1U, 1U);
    if (buf == NULL) {
        return false;
    }

    size_t offset = 0;
    const size_t start = _log_ring.count > limit ? _log_ring.count - limit : 0U;
    for (size_t i = start; i < _log_ring.count; ++i) {
        const size_t idx =
            (_log_ring.head + AVAR_DAEMON_LOG_RING_CAP - _log_ring.count + i) % AVAR_DAEMON_LOG_RING_CAP;
        const size_t len = strlen(_log_ring.lines[idx]);
        memcpy(buf + offset, _log_ring.lines[idx], len);
        offset += len;
        buf[offset++] = '\n';
    }

    *text_out = buf;
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

    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddNumberToObject(result, "queueCount", (double)queue_count());
    cJSON_AddNumberToObject(result, "activeDownloads", (double)download_active_count());
    cJSON_AddNumberToObject(result, "uptimeSeconds",
                            (double)(_daemon_started_at > 0 ? time(NULL) - _daemon_started_at : 0));
    return result;
}

static cJSON *handle_download_add(cJSON *params) {
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(params, "url");
    if (!cJSON_IsString(url) || url->valuestring == NULL) {
        return NULL;
    }

    const cJSON *queue = cJSON_GetObjectItemCaseSensitive(params, "queue");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, "name");
    const cJSON *attached = cJSON_GetObjectItemCaseSensitive(params, "attached");
    const bool is_attached = cJSON_IsTrue(attached);

    const char *queue_name =
        cJSON_IsString(queue) && queue->valuestring != NULL ? queue->valuestring : NULL;
    const char *dl_name = cJSON_IsString(name) && name->valuestring != NULL ? name->valuestring : NULL;

    cJSON *result = cJSON_CreateObject();
    if (result == NULL) {
        return NULL;
    }

    if (is_attached) {
        const int rc = transient_download(url->valuestring, queue_name, dl_name, true);
        cJSON_AddNumberToObject(result, "exitCode", rc);
        return result;
    }

    char *id = NULL;
    const int rc = download_start_background(url->valuestring, queue_name, dl_name, &id);
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
    unsigned limit = 50U;
    if (cJSON_IsNumber(max_lines) && max_lines->valuedouble > 0) {
        limit = (unsigned)max_lines->valuedouble;
    }

    char *text = NULL;
    if (!daemon_rpc_logs_fetch(cJSON_IsTrue(follow), limit, &text)) {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddStringToObject(result, "logs", text != NULL ? text : "");
    }
    free(text);
    return result;
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
    if (strcmp(method, "download.add") == 0) {
        return handle_download_add(params != NULL ? params : cJSON_CreateObject());
    }
    if (strcmp(method, "cli.exec") == 0) {
        return handle_cli_exec(params != NULL ? params : cJSON_CreateObject(), NULL);
    }
    if (strcmp(method, "logs.get") == 0) {
        return handle_logs_get(params != NULL ? params : cJSON_CreateObject());
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

bool daemon_rpc_handle(const char *request_json, char **response_json_out) {
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

bool daemon_rpc_handle_http(const char *uri, const char *body, const size_t body_len,
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

    if (strcmp(uri, "/api/ping") == 0) {
        *body_out = strdup("{\"status\":\"ok\"}\n");
        *status_out = 200;
        return *body_out != NULL;
    }

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
        if (!daemon_rpc_handle(request_buf, &rpc_response)) {
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
