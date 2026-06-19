#include <daemon/daemon_rpc.h>
#include <daemon/daemon_transport.h>
#include <gui_embed.h>
#include <logger.h>

#include <mongoose.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <poll.h>
    #include <pthread.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

typedef struct {
    struct mg_mgr mgr;
    bool started;
    bool https;
    const DaemonConfig *cfg;
} HttpTransportContext;

typedef struct {
    char endpoint[AVAR_CONFIG_PATH_MAX];
    bool started;
#if defined(_WIN32)
    HANDLE thread;
    volatile bool stop;
#else
    pthread_t thread;
    volatile bool stop;
    int listen_fd;
    int pipe_fd;
    volatile int active_fd;
#endif
} IpcTransportContext;

static void http_ev_handler(struct mg_connection *c, int ev, void *ev_data);
static bool ipc_write_frame(int fd, const char *json);
static bool ipc_read_frame(int fd, char **json_out);
static bool ipc_rpc_exchange(const char *endpoint, bool is_pipe, const char *request_json,
                             char **response_json_out, unsigned timeout_ms);

#if defined(_WIN32)
static DWORD WINAPI pipe_server_thread(LPVOID arg);
static bool win_pipe_configure(HANDLE pipe);
static HANDLE win_pipe_connect_client(const char *endpoint, unsigned timeout_ms);
#else
static void *ipc_server_thread(void *arg);
#endif

static bool http_start(DaemonTransport *self, const DaemonConfig *cfg) {
    HttpTransportContext *ctx = self->context;
    if (ctx == NULL || cfg == NULL) {
        return false;
    }

    char url[AVAR_DAEMON_URL_BUF_SIZE];
    const char *bind_addr =
        ctx->https ? cfg->server.https.bind_addr : cfg->server.http.bind_addr;
    const uint16_t port = ctx->https ? cfg->server.https.port : cfg->server.http.port;
    snprintf(url, sizeof url, "%s://%s:%u", ctx->https ? "https" : "http", bind_addr,
             (unsigned)port);

    mg_mgr_init(&ctx->mgr);
    struct mg_connection *listener = mg_http_listen(&ctx->mgr, url, http_ev_handler, (void *)cfg);
    if (listener == NULL) {
        LOG_ERROR("Failed to start HTTP daemon channel at %s", url);
        mg_mgr_free(&ctx->mgr);
        return false;
    }

#if defined(MG_TLS)
    if (ctx->https) {
        const DaemonHttpsChannel *tls = &cfg->server.https;
        if (tls->cert_file[0] != '\0' && tls->key_file[0] != '\0') {
            struct mg_tls_opts opts = {
                .cert = tls->cert_file,
                .key = tls->key_file,
            };
            mg_tls_init(listener, &opts);
        }
    }
#else
    (void)ctx;
#endif

    ctx->started = true;
    ctx->cfg = cfg;
    LOG_INFO("HTTP daemon channel listening at %s", url);
    return true;
}

static void http_stop(DaemonTransport *self) {
    HttpTransportContext *ctx = self->context;
    if (ctx == NULL || !ctx->started) {
        return;
    }
    mg_mgr_free(&ctx->mgr);
    ctx->started = false;
}

static bool http_ping(DaemonTransport *self) {
    (void)self;
    return true;
}

static void http_destroy(DaemonTransport *self) {
    http_stop(self);
    free(self->context);
    free(self);
}

static const DaemonTransportVTable http_vtable = {
    .start = http_start,
    .stop = http_stop,
    .ping = http_ping,
    .destroy = http_destroy,
};

static bool https_start(DaemonTransport *self, const DaemonConfig *cfg) {
    HttpTransportContext *ctx = self->context;
    if (ctx == NULL) {
        return false;
    }
    ctx->https = true;
    return http_start(self, cfg);
}

static const DaemonTransportVTable https_vtable = {
    .start = https_start,
    .stop = http_stop,
    .ping = http_ping,
    .destroy = http_destroy,
};

#if defined(_WIN32)
static bool pipe_start(DaemonTransport *self, const DaemonConfig *cfg) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || cfg == NULL) {
        return false;
    }

    snprintf(ctx->endpoint, sizeof ctx->endpoint, "%s", cfg->server.pipe.name);
    ctx->stop = false;
    ctx->thread = CreateThread(NULL, 0, pipe_server_thread, ctx, 0, NULL);
    if (ctx->thread == NULL) {
        LOG_ERROR("Failed to start named-pipe listener");
        return false;
    }

    ctx->started = true;
    LOG_INFO("Named-pipe daemon channel listening at '%s'", ctx->endpoint);
    return true;
}

static void pipe_stop(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || !ctx->started) {
        return;
    }
    ctx->stop = true;
    HANDLE probe = CreateFileA(ctx->endpoint, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (probe != INVALID_HANDLE_VALUE) {
        CloseHandle(probe);
    }
    WaitForSingleObject(ctx->thread, 5000);
    CloseHandle(ctx->thread);
    ctx->started = false;
}

static bool win_pipe_configure(const HANDLE pipe) {
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    return SetNamedPipeHandleState(pipe, &mode, NULL, NULL) != 0;
}

static HANDLE win_pipe_connect_client(const char *endpoint, const unsigned timeout_ms) {
    if (endpoint == NULL) {
        return INVALID_HANDLE_VALUE;
    }

    const DWORD wait_ms = timeout_ms == 0U ? 5000U : timeout_ms;
    const uint64_t deadline = GetTickCount64() + (uint64_t)wait_ms;
    bool pipe_ready = false;
    while (GetTickCount64() < deadline) {
        if (WaitNamedPipeA(endpoint, 50)) {
            pipe_ready = true;
            break;
        }
        const DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PIPE_BUSY) {
            LOG_DEBUG("WaitNamedPipe failed for '%s' (error %lu)", endpoint, err);
            return INVALID_HANDLE_VALUE;
        }
        Sleep(10);
    }
    if (!pipe_ready) {
        LOG_DEBUG("WaitNamedPipe timed out for '%s'", endpoint);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE pipe = CreateFileA(endpoint, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                              NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        LOG_DEBUG("CreateFile failed for pipe '%s' (error %lu)", endpoint, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    if (!win_pipe_configure(pipe)) {
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }

    return pipe;
}

static DWORD WINAPI pipe_server_thread(LPVOID arg) {
    IpcTransportContext *ctx = (IpcTransportContext *)arg;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), &sd, FALSE};

    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE)) {
        LOG_ERROR("Failed to configure named-pipe security");
        return 1;
    }

    HANDLE pipe = CreateNamedPipeA(
        ctx->endpoint, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0,
        &sa);
    while (!ctx->stop) {
        if (pipe == INVALID_HANDLE_VALUE) {
            LOG_WARNING("CreateNamedPipe failed for '%s' (error %lu)", ctx->endpoint,
                        GetLastError());
            Sleep(100);
            pipe = CreateNamedPipeA(
                ctx->endpoint, PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536,
                65536, 0, &sa);
            continue;
        }

        (void)win_pipe_configure(pipe);

        const BOOL connected = ConnectNamedPipe(pipe, NULL)
                                   ? TRUE
                                   : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            LOG_DEBUG("ConnectNamedPipe failed (error %lu)", GetLastError());
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
            continue;
        }

        char *request = NULL;
        if (ipc_read_frame((int)(intptr_t)pipe, &request) && request != NULL) {
            char *response = NULL;
            if (daemon_rpc_handle(request, &response) && response != NULL) {
                if (!ipc_write_frame((int)(intptr_t)pipe, response)) {
                    LOG_DEBUG("Pipe RPC response write failed");
                }
                free(response);
            } else {
                LOG_DEBUG("Pipe RPC request handling failed");
            }
            free(request);
        } else {
            LOG_DEBUG("Pipe RPC request read failed");
        }

        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);

        pipe = CreateNamedPipeA(
            ctx->endpoint, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536,
            0, &sa);
    }

    return 0;
}

static bool pipe_ping(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL) {
        return false;
    }

    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
    char *resp = NULL;
    const bool ok = ipc_rpc_exchange(ctx->endpoint, true, req, &resp, 5000U);
    free(resp);
    return ok;
}

static void pipe_destroy(DaemonTransport *self) {
    pipe_stop(self);
    free(self->context);
    free(self);
}

static const DaemonTransportVTable pipe_vtable = {
    .start = pipe_start,
    .stop = pipe_stop,
    .ping = pipe_ping,
    .destroy = pipe_destroy,
};

static bool unix_start_stub(DaemonTransport *self, const DaemonConfig *cfg) {
    (void)self;
    (void)cfg;
    LOG_ERROR("Unix-socket daemon channel is not supported on Windows");
    return false;
}

static void unix_stop_stub(DaemonTransport *self) {
    (void)self;
}

static bool unix_ping_stub(DaemonTransport *self) {
    (void)self;
    return false;
}

static void unix_destroy_stub(DaemonTransport *self) {
    free(self->context);
    free(self);
}

static const DaemonTransportVTable unix_vtable = {
    .start = unix_start_stub,
    .stop = unix_stop_stub,
    .ping = unix_ping_stub,
    .destroy = unix_destroy_stub,
};

#else

static bool ipc_server_loop(IpcTransportContext *ctx, bool is_pipe) {
    while (!ctx->stop) {
        int client_fd = -1;
        if (is_pipe) {
            if (ctx->pipe_fd < 0) {
                break;
            }

            struct pollfd pfd = {.fd = ctx->pipe_fd, .events = POLLIN};
            const int ready = poll(&pfd, 1, 100);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (ready == 0) {
                continue;
            }

            client_fd = ctx->pipe_fd;
        } else {
            struct pollfd pfd = {.fd = ctx->listen_fd, .events = POLLIN};
            const int ready = poll(&pfd, 1, 100);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (ready == 0) {
                continue;
            }

            client_fd = accept(ctx->listen_fd, NULL, NULL);
            if (client_fd < 0) {
                if (ctx->stop) {
                    break;
                }
                continue;
            }
        }

        char *request = NULL;
        ctx->active_fd = client_fd;
        if (ipc_read_frame(client_fd, &request) && request != NULL) {
            char *response = NULL;
            if (daemon_rpc_handle(request, &response) && response != NULL) {
                (void)ipc_write_frame(client_fd, response);
                free(response);
            }
            free(request);
        }

        ctx->active_fd = -1;
        if (!is_pipe) {
            close(client_fd);
        }
    }
    return true;
}

static void *ipc_server_thread(void *arg) {
    IpcTransportContext *ctx = (IpcTransportContext *)arg;
    const bool is_pipe = ctx->listen_fd < 0;
    (void)ipc_server_loop(ctx, is_pipe);
    return NULL;
}

static bool pipe_start(DaemonTransport *self, const DaemonConfig *cfg) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || cfg == NULL) {
        return false;
    }

    snprintf(ctx->endpoint, sizeof ctx->endpoint, "%s", cfg->server.pipe.name);
    (void)unlink(ctx->endpoint);
    if (mkfifo(ctx->endpoint, 0600) != 0 && errno != EEXIST) {
        LOG_ERROR("Failed to create FIFO '%s'", ctx->endpoint);
        return false;
    }

    ctx->pipe_fd = open(ctx->endpoint, O_RDWR);
    if (ctx->pipe_fd < 0) {
        LOG_ERROR("Failed to open FIFO '%s'", ctx->endpoint);
        (void)unlink(ctx->endpoint);
        return false;
    }

    ctx->pipe_fd = -1;
    ctx->listen_fd = -1;
    ctx->active_fd = -1;
    ctx->stop = false;
    if (pthread_create(&ctx->thread, NULL, ipc_server_thread, ctx) != 0) {
        LOG_ERROR("Failed to start FIFO listener thread");
        return false;
    }

    ctx->started = true;
    LOG_INFO("FIFO daemon channel listening at '%s'", ctx->endpoint);
    return true;
}

static void pipe_stop(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || !ctx->started) {
        return;
    }
    ctx->stop = true;
    if (ctx->active_fd >= 0) {
        shutdown(ctx->active_fd, SHUT_RDWR);
    }
    if (ctx->pipe_fd >= 0) {
        shutdown(ctx->pipe_fd, SHUT_RDWR);
    }
    pthread_join(ctx->thread, NULL);
    if (ctx->pipe_fd >= 0) {
        close(ctx->pipe_fd);
        ctx->pipe_fd = -1;
    }
    unlink(ctx->endpoint);
    ctx->started = false;
}

static bool pipe_ping(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL) {
        return false;
    }

    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
    char *resp = NULL;
    const bool ok = ipc_rpc_exchange(ctx->endpoint, true, req, &resp, 5000U);
    free(resp);
    return ok;
}

static void pipe_destroy(DaemonTransport *self) {
    pipe_stop(self);
    free(self->context);
    free(self);
}

static const DaemonTransportVTable pipe_vtable = {
    .start = pipe_start,
    .stop = pipe_stop,
    .ping = pipe_ping,
    .destroy = pipe_destroy,
};

static bool unix_start(DaemonTransport *self, const DaemonConfig *cfg) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || cfg == NULL) {
        return false;
    }

    snprintf(ctx->endpoint, sizeof ctx->endpoint, "%s", cfg->server.unix_socket.path);
    (void)unlink(ctx->endpoint);

    ctx->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->listen_fd < 0) {
        LOG_ERROR("Failed to create unix socket");
        return false;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", ctx->endpoint);
    if (bind(ctx->listen_fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        LOG_ERROR("Failed to bind unix socket '%s'", ctx->endpoint);
        close(ctx->listen_fd);
        return false;
    }

    if (listen(ctx->listen_fd, 8) != 0) {
        LOG_ERROR("Failed to listen on unix socket");
        close(ctx->listen_fd);
        return false;
    }

    ctx->stop = false;
    ctx->active_fd = -1;
    if (pthread_create(&ctx->thread, NULL, ipc_server_thread, ctx) != 0) {
        LOG_ERROR("Failed to start unix socket listener thread");
        close(ctx->listen_fd);
        return false;
    }

    ctx->started = true;
    LOG_INFO("Unix-socket daemon channel listening at '%s'", ctx->endpoint);
    return true;
}

static void unix_stop(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL || !ctx->started) {
        return;
    }
    ctx->stop = true;
    if (ctx->active_fd >= 0) {
        shutdown(ctx->active_fd, SHUT_RDWR);
    }
    shutdown(ctx->listen_fd, SHUT_RDWR);
    pthread_join(ctx->thread, NULL);
    close(ctx->listen_fd);
    unlink(ctx->endpoint);
    ctx->started = false;
}

static bool unix_ping(DaemonTransport *self) {
    IpcTransportContext *ctx = self->context;
    if (ctx == NULL) {
        return false;
    }

    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
    char *resp = NULL;
    const bool ok = ipc_rpc_exchange(ctx->endpoint, false, req, &resp, 5000U);
    free(resp);
    return ok;
}

static void unix_destroy(DaemonTransport *self) {
    unix_stop(self);
    free(self->context);
    free(self);
}

static const DaemonTransportVTable unix_vtable = {
    .start = unix_start,
    .stop = unix_stop,
    .ping = unix_ping,
    .destroy = unix_destroy,
};

#endif

static bool http_uri_is_api(const struct mg_http_message *hm) {
    if (hm == NULL || hm->uri.len < 4) {
        return false;
    }
    if (memcmp(hm->uri.buf, "/api", 4) != 0) {
        return false;
    }
    return hm->uri.len == 4 || hm->uri.buf[4] == '/' || hm->uri.buf[4] == '?';
}

static const char *http_cors_allow_origin(const DaemonCorsConfig *cors,
                                          struct mg_http_message *hm) {
    if (cors == NULL || cors->allow_origin[0] == '\0') {
        return "*";
    }
    if (strcmp(cors->allow_origin, "*") == 0) {
        return "*";
    }

    if (hm != NULL) {
        struct mg_str *origin = mg_http_get_header(hm, "Origin");
        if (origin != NULL && mg_strcmp(*origin, mg_str(cors->allow_origin)) == 0) {
            return cors->allow_origin;
        }
    }

    return cors->allow_origin;
}

static size_t http_write_cors_headers(char *buf, size_t buflen, const DaemonCorsConfig *cors,
                                      struct mg_http_message *hm) {
    if (cors == NULL || !cors->enabled) {
        return 0;
    }

    return (size_t)snprintf(buf, buflen,
                            "Access-Control-Allow-Origin: %s\r\n"
                            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                            "Access-Control-Expose-Headers: Content-Type\r\n",
                            http_cors_allow_origin(cors, hm));
}

static void http_reply_json(struct mg_connection *c, int status, const char *body,
                            const DaemonCorsConfig *cors, struct mg_http_message *hm) {
    char headers[512];
    size_t off = (size_t)snprintf(headers, sizeof headers,
                                  "Content-Type: application/json; charset=utf-8\r\n");
    off += http_write_cors_headers(headers + off, sizeof headers - off, cors, hm);
    mg_http_reply(c, status, headers, "%s", body);
}

static void http_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_CLOSE) {
        daemon_rpc_stream_detach(c);
        return;
    }

    if (ev == MG_EV_WS_OPEN) {
        daemon_rpc_stream_attach_ws(c);
        return;
    }

    if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        if (wm != NULL && wm->data.len >= 4U &&
            memcmp(wm->data.buf, "ping", 4) == 0) {
            daemon_rpc_note_frontend_activity();
            mg_ws_send(c, "pong", 4, WEBSOCKET_OP_TEXT);
        }
        return;
    }

    if (ev != MG_EV_HTTP_MSG) {
        return;
    }

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    const DaemonConfig *cfg = (const DaemonConfig *)c->fn_data;
    const DaemonCorsConfig *cors = cfg != NULL ? &cfg->server.cors : NULL;

    if (cors != NULL && cors->enabled && http_uri_is_api(hm) &&
        mg_strcasecmp(hm->method, mg_str("OPTIONS")) == 0) {
        char headers[512];
        size_t off = http_write_cors_headers(headers, sizeof headers, cors, hm);
        snprintf(headers + off, sizeof headers - off, "Access-Control-Max-Age: 86400\r\n");
        mg_http_reply(c, 204, headers, "");
        return;
    }

    if (!daemon_rpc_check_http_auth(hm)) {
        http_reply_json(c, 401, "{\"error\":\"unauthorized\"}\n", cors, hm);
        return;
    }

#if defined(AVAR_EMBED_GUI)
    if (!http_uri_is_api(hm) && gui_embed_available()) {
        char uri[512];
        snprintf(uri, sizeof uri, "%.*s", (int)hm->uri.len, hm->uri.buf);
        if (gui_embed_serve(c, uri)) {
            return;
        }
    }
#endif

    if (mg_match(hm->uri, mg_str("/api/events"), NULL)) {
        char headers[512];
        size_t off = (size_t)snprintf(headers, sizeof headers,
                                      "Content-Type: text/event-stream\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: keep-alive\r\n");
        off += http_write_cors_headers(headers + off, sizeof headers - off, cors, hm);
        mg_printf(c, "HTTP/1.1 200 OK\r\n%s\r\n", headers);
        daemon_rpc_stream_attach_sse(c);
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/ws"), NULL)) {
        mg_ws_upgrade(c, hm, NULL);
        return;
    }

    char uri[256];
    snprintf(uri, sizeof uri, "%.*s", (int)hm->uri.len, hm->uri.buf);

    char *body = NULL;
    int status = 500;
    if (!daemon_rpc_handle_http(uri, hm->body.buf, hm->body.len, &body, &status)) {
        http_reply_json(c, 500, "{\"error\":\"internal\"}\n", cors, hm);
        return;
    }

    http_reply_json(c, status, body, cors, hm);
    free(body);
}

static bool ipc_write_frame(const int fd, const char *json) {
    if (json == NULL) {
        return false;
    }

    const uint32_t len = (uint32_t)strlen(json);
    const uint8_t header[4] = {(uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8),
                               (uint8_t)len};

#if defined(_WIN32)
    DWORD written = 0;
    if (!WriteFile((HANDLE)(intptr_t)fd, header, 4, &written, NULL) || written != 4) {
        return false;
    }
    if (!WriteFile((HANDLE)(intptr_t)fd, json, len, &written, NULL) || written != len) {
        return false;
    }
    (void)FlushFileBuffers((HANDLE)(intptr_t)fd);
    return true;
#else
    if (write(fd, header, 4) != 4) {
        return false;
    }
    if (write(fd, json, len) != (ssize_t)len) {
        return false;
    }
    return true;
#endif
}

static bool ipc_read_frame(const int fd, char **json_out) {
    if (json_out != NULL) {
        *json_out = NULL;
    }

    uint8_t header[4];
#if defined(_WIN32)
    DWORD read_count = 0;
    if (!ReadFile((HANDLE)(intptr_t)fd, header, 4, &read_count, NULL) || read_count != 4) {
        return false;
    }
#else
    if (read(fd, header, 4) != 4) {
        return false;
    }
#endif

    const uint32_t len =
        ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) |
        (uint32_t)header[3];
    if (len == 0U || len >= AVAR_DAEMON_RPC_FRAME_MAX) {
        return false;
    }

    char *buf = calloc((size_t)len + 1U, 1U);
    if (buf == NULL) {
        return false;
    }

#if defined(_WIN32)
    if (!ReadFile((HANDLE)(intptr_t)fd, buf, len, &read_count, NULL) || read_count != len) {
        free(buf);
        return false;
    }
#else
    size_t total = 0U;
    while (total < len) {
        const ssize_t n = read(fd, buf + total, (size_t)len - total);
        if (n <= 0) {
            free(buf);
            return false;
        }
        total += (size_t)n;
    }
#endif

    if (json_out != NULL) {
        *json_out = buf;
    } else {
        free(buf);
    }
    return true;
}

#if defined(_WIN32)
static bool ipc_rpc_exchange(const char *endpoint, const bool is_pipe, const char *request_json,
                             char **response_json_out, const unsigned timeout_ms) {
    (void)is_pipe;

    if (endpoint == NULL || request_json == NULL || response_json_out == NULL) {
        return false;
    }
    *response_json_out = NULL;

    HANDLE pipe = win_pipe_connect_client(endpoint, timeout_ms);
    if (pipe == INVALID_HANDLE_VALUE) {
        LOG_DEBUG("Pipe RPC connect failed for '%s'", endpoint);
        return false;
    }

    if (!ipc_write_frame((int)(intptr_t)pipe, request_json)) {
        LOG_DEBUG("Pipe RPC write failed for '%s'", endpoint);
        CloseHandle(pipe);
        return false;
    }

    if (!ipc_read_frame((int)(intptr_t)pipe, response_json_out)) {
        LOG_DEBUG("Pipe RPC read failed for '%s'", endpoint);
        CloseHandle(pipe);
        return false;
    }

    CloseHandle(pipe);
    return true;
}
#else
static bool ipc_read_exact_timeout(int fd, void *buf, const size_t len, uint64_t deadline_ms) {
    size_t total = 0U;
    while (total < len) {
        const uint64_t now = mg_millis();
        if (now >= deadline_ms) {
            return false;
        }

        unsigned wait_ms = (unsigned)(deadline_ms - now);
        if (wait_ms > 200U) {
            wait_ms = 200U;
        }

        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        const int ready = poll(&pfd, 1, (int)wait_ms);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (ready == 0) {
            continue;
        }

        const ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) {
            return false;
        }
        total += (size_t)n;
    }

    return true;
}

static bool ipc_read_frame_timeout(int fd, char **json_out, uint64_t deadline_ms) {
    if (json_out != NULL) {
        *json_out = NULL;
    }

    uint8_t header[4];
    if (!ipc_read_exact_timeout(fd, header, sizeof header, deadline_ms)) {
        return false;
    }

    const uint32_t len =
        ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) |
        (uint32_t)header[3];
    if (len == 0U || len >= AVAR_DAEMON_RPC_FRAME_MAX) {
        return false;
    }

    char *buf = calloc((size_t)len + 1U, 1U);
    if (buf == NULL) {
        return false;
    }

    if (!ipc_read_exact_timeout(fd, buf, len, deadline_ms)) {
        free(buf);
        return false;
    }

    if (json_out != NULL) {
        *json_out = buf;
    } else {
        free(buf);
    }
    return true;
}

static int ipc_open_endpoint(const char *endpoint, const bool is_pipe, const unsigned timeout_ms) {
    if (endpoint == NULL) {
        return -1;
    }

    const unsigned wait_ms = timeout_ms == 0U ? 5000U : timeout_ms;
    const uint64_t deadline = mg_millis() + (uint64_t)wait_ms;

    while (mg_millis() < deadline) {
        int fd = -1;
        if (is_pipe) {
            fd = open(endpoint, O_RDWR | O_NONBLOCK);
            if (fd < 0 && (errno == ENXIO || errno == ENOENT || errno == EAGAIN)) {
                usleep(10000);
                continue;
            }
        } else {
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) {
                return -1;
            }

            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }

            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof addr.sun_path, "%s", endpoint);
            if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
                const int connect_err = errno;
                close(fd);
                if (connect_err == EINPROGRESS || connect_err == EAGAIN) {
                    usleep(10000);
                    continue;
                }
                return -1;
            }
        }

        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        }

        return fd;
    }

    return -1;
}

static bool ipc_rpc_exchange(const char *endpoint, const bool is_pipe, const char *request_json,
                             char **response_json_out, const unsigned timeout_ms) {
    if (endpoint == NULL || request_json == NULL || response_json_out == NULL) {
        return false;
    }
    *response_json_out = NULL;

    const int fd = ipc_open_endpoint(endpoint, is_pipe, timeout_ms);
    if (fd < 0) {
        return false;
    }

    const unsigned wait_ms = timeout_ms == 0U ? 5000U : timeout_ms;
    const uint64_t deadline = mg_millis() + (uint64_t)wait_ms;

    if (!ipc_write_frame(fd, request_json)) {
        close(fd);
        return false;
    }

    const bool ok = ipc_read_frame_timeout(fd, response_json_out, deadline);
    close(fd);
    return ok;
}
#endif

typedef struct {
    struct mg_mgr mgr;
    bool done;
    bool ok;
    bool request_sent;
    char *response;
    const char *request;
    const char *auth_token;
    char url[AVAR_DAEMON_URL_BUF_SIZE];
} HttpClientContext;

static void http_client_capture_response(HttpClientContext *ctx, struct mg_http_message *hm) {
    if (ctx == NULL || hm == NULL || ctx->ok) {
        return;
    }

    if (hm->body.len > 0U) {
        free(ctx->response);
        ctx->response = calloc(hm->body.len + 1U, 1U);
        if (ctx->response != NULL) {
            memcpy(ctx->response, hm->body.buf, hm->body.len);
        }
    }

    ctx->ok = true;
    ctx->done = true;
}

static void http_client_send_request(struct mg_connection *c, HttpClientContext *ctx) {
    if (ctx == NULL || ctx->request_sent) {
        return;
    }

    ctx->request_sent = true;

    const struct mg_str host = mg_url_host(ctx->url);
    const char *uri = mg_url_uri(ctx->url);
    const size_t body_len = ctx->request != NULL ? strlen(ctx->request) : 0U;

    if (ctx->auth_token != NULL && ctx->auth_token[0] != '\0') {
        mg_printf(c,
                  "POST %s HTTP/1.1\r\n"
                  "Host: %.*s\r\n"
                  "Content-Type: application/json\r\n"
                  "Authorization: Bearer %s\r\n"
                  "Connection: close\r\n"
                  "Content-Length: %zu\r\n"
                  "\r\n",
                  uri, (int)host.len, host.buf, ctx->auth_token, body_len);
    } else {
        mg_printf(c,
                  "POST %s HTTP/1.1\r\n"
                  "Host: %.*s\r\n"
                  "Content-Type: application/json\r\n"
                  "Connection: close\r\n"
                  "Content-Length: %zu\r\n"
                  "\r\n",
                  uri, (int)host.len, host.buf, body_len);
    }

    if (ctx->request != NULL && body_len > 0U) {
        mg_send(c, ctx->request, body_len);
    }
}

static void http_client_handler(struct mg_connection *c, int ev, void *ev_data) {
    HttpClientContext *ctx = (HttpClientContext *)c->fn_data;
    if (ctx == NULL) {
        return;
    }

    if (ev == MG_EV_CONNECT) {
        if (ev_data != NULL && *(int *)ev_data != 0) {
            ctx->done = true;
            c->is_closing = 1;
            return;
        }
        http_client_send_request(c, ctx);
        return;
    }

    if (ev == MG_EV_WRITE) {
        if (!ctx->request_sent && !c->is_connecting && !c->is_tls_hs) {
            http_client_send_request(c, ctx);
        }
        return;
    }

    if (ev == MG_EV_HTTP_MSG) {
        http_client_capture_response(ctx, (struct mg_http_message *)ev_data);
        c->is_closing = 1;
        return;
    }

    if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
        if (!ctx->ok) {
            ctx->done = true;
        }
    }
}

static bool http_rpc_request_timeout(const DaemonConfig *cfg, const char *request_json,
                                     char **response_json_out, const unsigned timeout_ms) {
    if (cfg == NULL || request_json == NULL || response_json_out == NULL) {
        return false;
    }

    const char *host =
        cfg->session.remote_host[0] != '\0' ? cfg->session.remote_host : "127.0.0.1";
    const uint16_t port =
        cfg->session.remote_port != 0 ? cfg->session.remote_port : cfg->server.http.port;

    char url[AVAR_DAEMON_URL_BUF_SIZE];
    snprintf(url, sizeof url, "http://%s:%u/api/rpc", host, (unsigned)port);

    HttpClientContext ctx = {
        .request = request_json,
        .auth_token = cfg->server.auth_token[0] != '\0' ? cfg->server.auth_token : NULL,
    };
    snprintf(ctx.url, sizeof ctx.url, "%s", url);
    mg_mgr_init(&ctx.mgr);

    struct mg_connection *c = mg_http_connect(&ctx.mgr, ctx.url, http_client_handler, &ctx);
    if (c == NULL) {
        mg_mgr_free(&ctx.mgr);
        return false;
    }

    const unsigned wait_ms = timeout_ms == 0U ? 5000U : timeout_ms;
    const uint64_t deadline = mg_millis() + (uint64_t)wait_ms;
    while (!ctx.done && mg_millis() < deadline) {
        mg_mgr_poll(&ctx.mgr, 50);
    }

    mg_mgr_free(&ctx.mgr);
    if (!ctx.ok || ctx.response == NULL) {
        free(ctx.response);
        return false;
    }

    *response_json_out = ctx.response;
    return true;
}

static bool http_rpc_request(const DaemonConfig *cfg, const char *request_json,
                             char **response_json_out) {
    return http_rpc_request_timeout(cfg, request_json, response_json_out, 5000U);
}

static bool http_ping_remote_timeout(const DaemonConfig *cfg, const unsigned timeout_ms) {
    char *response = NULL;
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
    const bool ok = http_rpc_request_timeout(cfg, req, &response, timeout_ms);
    free(response);
    return ok;
}

static bool http_ping_remote(const DaemonConfig *cfg) {
    return http_ping_remote_timeout(cfg, 5000U);
}

DaemonTransport *daemon_transport_create(AvarTransportKind kind) {
    DaemonTransport *transport = calloc(1, sizeof(DaemonTransport));
    if (transport == NULL) {
        return NULL;
    }

    switch (kind) {
        case AvarTransportHttp: {
            HttpTransportContext *ctx = calloc(1, sizeof(HttpTransportContext));
            if (ctx == NULL) {
                free(transport);
                return NULL;
            }
            transport->context = ctx;
            transport->vtable = &http_vtable;
            break;
        }
        case AvarTransportPipe: {
            IpcTransportContext *ctx = calloc(1, sizeof(IpcTransportContext));
            if (ctx == NULL) {
                free(transport);
                return NULL;
            }
            transport->context = ctx;
            transport->vtable = &pipe_vtable;
            break;
        }
        case AvarTransportUnix: {
            IpcTransportContext *ctx = calloc(1, sizeof(IpcTransportContext));
            if (ctx == NULL) {
                free(transport);
                return NULL;
            }
            transport->context = ctx;
            transport->vtable = &unix_vtable;
            break;
        }
        case AvarTransportLocal:
            free(transport);
            return NULL;
    }

    return transport;
}

bool daemon_transport_start(DaemonTransport *transport, const DaemonConfig *cfg) {
    if (transport == NULL || transport->vtable == NULL || transport->vtable->start == NULL) {
        return false;
    }
    return transport->vtable->start(transport, cfg);
}

void daemon_transport_stop(DaemonTransport *transport) {
    if (transport != NULL && transport->vtable != NULL && transport->vtable->stop != NULL) {
        transport->vtable->stop(transport);
    }
}

bool daemon_transport_ping(DaemonTransport *transport) {
    if (transport == NULL || transport->vtable == NULL || transport->vtable->ping == NULL) {
        return false;
    }
    return transport->vtable->ping(transport);
}

void daemon_transport_destroy(DaemonTransport *transport) {
    if (transport != NULL && transport->vtable != NULL && transport->vtable->destroy != NULL) {
        transport->vtable->destroy(transport);
    }
}

void daemon_transport_poll(DaemonTransport *transport, unsigned timeout_ms) {
    if (transport == NULL || transport->vtable != &http_vtable) {
        return;
    }

    HttpTransportContext *ctx = transport->context;
    if (ctx != NULL && ctx->started) {
        mg_mgr_poll(&ctx->mgr, (int)timeout_ms);
        daemon_rpc_streams_tick(&ctx->mgr);
    }
}

static bool ping_transport_timeout(const AvarTransportKind kind, const DaemonConfig *cfg,
                                 const unsigned timeout_ms) {
    if (cfg == NULL) {
        return false;
    }

    const unsigned wait_ms = timeout_ms == 0U ? 5000U : timeout_ms;

    switch (kind) {
        case AvarTransportHttp:
            return cfg->server.http.enabled && http_ping_remote_timeout(cfg, wait_ms);
        case AvarTransportPipe: {
            if (!cfg->server.pipe.enabled) {
                return false;
            }
            char *resp = NULL;
            const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
            const bool ok =
                ipc_rpc_exchange(cfg->server.pipe.name, true, req, &resp, wait_ms);
            free(resp);
            return ok;
        }
        case AvarTransportUnix: {
            if (!cfg->server.unix_socket.enabled) {
                return false;
            }
            char *resp = NULL;
            const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
            const bool ok =
                ipc_rpc_exchange(cfg->server.unix_socket.path, false, req, &resp, wait_ms);
            free(resp);
            return ok;
        }
        default:
            return false;
    }
}

static bool ping_transport(const AvarTransportKind kind, const DaemonConfig *cfg) {
    return ping_transport_timeout(kind, cfg, 5000U);
}

bool daemon_transport_ping_remote(const AvarTransportKind kind, const DaemonConfig *cfg) {
    return ping_transport(kind, cfg);
}

bool daemon_transport_ping_remote_timeout(const AvarTransportKind kind, const DaemonConfig *cfg,
                                          const unsigned timeout_ms) {
    return ping_transport_timeout(kind, cfg, timeout_ms);
}

bool daemon_transport_ping_any_timeout(const DaemonConfig *cfg, const unsigned timeout_ms) {
    if (cfg == NULL) {
        return false;
    }

    const AvarTransportKind order[] = {cfg->session.transport, AvarTransportPipe,
                                       AvarTransportHttp, AvarTransportUnix};

    for (size_t i = 0; i < sizeof order / sizeof order[0]; ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) {
            if (order[j] == order[i]) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && ping_transport_timeout(order[i], cfg, timeout_ms)) {
            return true;
        }
    }

    return false;
}

bool daemon_transport_ping_any(const DaemonConfig *cfg) {
    return daemon_transport_ping_any_timeout(cfg, 5000U);
}

DaemonTransport *daemon_transport_create_https(void) {
    DaemonTransport *transport = calloc(1, sizeof(DaemonTransport));
    if (transport == NULL) {
        return NULL;
    }

    HttpTransportContext *ctx = calloc(1, sizeof(HttpTransportContext));
    if (ctx == NULL) {
        free(transport);
        return NULL;
    }

    ctx->https = true;
    transport->context = ctx;
    transport->vtable = &https_vtable;
    return transport;
}

static bool rpc_transport(const AvarTransportKind kind, const DaemonConfig *cfg,
                          const char *request_json, char **response_json_out) {
    if (cfg == NULL || request_json == NULL || response_json_out == NULL) {
        return false;
    }

    switch (kind) {
        case AvarTransportHttp:
            return cfg->server.http.enabled &&
                   http_rpc_request(cfg, request_json, response_json_out);
        case AvarTransportPipe:
            return cfg->server.pipe.enabled &&
                   ipc_rpc_exchange(cfg->server.pipe.name, true, request_json, response_json_out,
                                    5000U);
        case AvarTransportUnix:
            return cfg->server.unix_socket.enabled &&
                   ipc_rpc_exchange(cfg->server.unix_socket.path, false, request_json,
                                    response_json_out, 5000U);
        default:
            return false;
    }
}

bool daemon_transport_rpc_request(const AvarTransportKind kind, const DaemonConfig *cfg,
                                  const char *request_json, char **response_json_out) {
    return rpc_transport(kind, cfg, request_json, response_json_out);
}

bool daemon_transport_rpc_request_any(const DaemonConfig *cfg, const char *request_json,
                                      char **response_json_out) {
    if (cfg == NULL) {
        return false;
    }

    const AvarTransportKind order[] = {cfg->session.transport, AvarTransportPipe,
                                       AvarTransportHttp, AvarTransportUnix};

    for (size_t i = 0; i < sizeof order / sizeof order[0]; ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) {
            if (order[j] == order[i]) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && rpc_transport(order[i], cfg, request_json, response_json_out)) {
            return true;
        }
    }

    return false;
}
