#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mongoose.h>

#include <avar.h>
#include <daemon.h>
#include <io.h>

// #include <sys/types.h>
// #include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
/* ctrl_c_handler.c */
#include <stdio.h>
#ifdef _WIN32                /* Windows specific headers   */
#include <windows.h>
#else                         /* POSIX (Linux, macOS…)     */
#include <signal.h>
#endif

/* User supplied function that will be called when Ctrl+C is pressed.  */
static void (*user_handler)(void) = NULL;

static void handle_ctrl_c();

static int install_ctrl_c_handler(void (*handler)(void));

static void ev_handler(struct mg_connection *c, int ev, void *ev_data);

static int loglevel_to_mg_ll(log_level_t logLevel);

static void mg_log_to_log(char c, void *param);


/* Path of the Unix domain socket file */
static stringa SOCK_PATH = "/tmp/avar.sock";
static volatile bool _runDaemon = false;
static struct mg_mgr g_mg_mgr;

void set_socket_path(stringa path) {
    SOCK_PATH = path;
}

int start_daemon() {
    // Set
    mg_log_set(loglevel_to_mg_ll(get_log_level()));
    // mg_log_set_fn(mg_log_to_log, nullptr);

    int port = 8000;
    LOG_INFO("Starting HTTP API server on port %d...", port);
    char url[32];
    snprintf(url, sizeof(url), "http://0.0.0.0:%d", port);

    mg_mgr_init(&g_mg_mgr); // Initialise event manager
    mg_http_listen(&g_mg_mgr, url, ev_handler, NULL); // Setup listener

    _runDaemon = true;
    install_ctrl_c_handler(handle_ctrl_c);

    LOG_INFO("Started HTTP API server at %s", url);

    while (_runDaemon) {
        // Run an infinite event loop
        mg_mgr_poll(&g_mg_mgr, 1000);
    }

    return EXIT_SUCCESS;
}

int stop_daemon() {
    return EXIT_FATAL_ERROR;
}

int restart_daemon() {
    if (stop_daemon() == EXIT_SUCCESS) {
        return start_daemon();
    }

    return EXIT_FATAL_ERROR;
}

// int start_daemon1() {
//     struct sigaction sa;
//     sa.sa_handler = sig_handler;
//     sigemptyset(&sa.sa_mask);
//     sa.sa_flags = 0;
//     sigaction(SIGINT, &sa, NULL);
//     sigaction(SIGTERM, &sa, NULL);
//
//     int client_fd;
//     ssize_t nread, nwritten;
//     char buf[1024];
//
//     /* Register cleanup in case of abnormal exit (e.g., SIGSEGV) */
//     atexit(cleanup);
//
//     /* Set up signal handlers as shown above ... */
//
//     server_fd = setup_server_socket();
//     if (server_fd == -1)
//         return EXIT_FAILURE;
//
//     printf("Unix domain socket server listening on %s\n", SOCK_PATH);
//
//     for (;;) {
//         client_fd = accept(server_fd, NULL, NULL);
//         if (client_fd == -1) {
//             /* If we were interrupted by a signal, try again */
//             if (errno == EINTR)
//                 continue;
//             perror("accept");
//             break;   /* fatal error – exit loop */
//         }
//
//         printf("Accepted new client\n");
//
//         /* Simple echo protocol: read -> send back */
//         while ((nread = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
//             nwritten = send(client_fd, buf, nread, 0);
//             if (nwritten != nread) {
//                 perror("send");
//                 break;
//             }
//         }
//
//         if (nread == -1)
//             perror("recv");
//
//         close(client_fd);
//         printf("Client disconnected\n");
//     }
//
//     /* If we reach here, something went wrong */
//     cleanup();
//     return EXIT_FAILURE;
// }

// void sig_handler(int signo)
// {
//     (void)signo;   /* unused */
//     cleanup();
//     exit(EXIT_SUCCESS);
// }

// int setup_server_socket(void)
// {
//     struct sockaddr_un addr;
//     int fd;
//
//     /* Create a Unix domain stream socket */
//     if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
//         perror("socket");
//         return -1;
//     }
//
//     /* Make sure the socket file does not already exist */
//     unlink(SOCK_PATH);
//
//     memset(&addr, 0, sizeof(struct sockaddr_un));
//     addr.sun_family = AF_UNIX;
//     strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path)-1);
//
//     if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
//         perror("bind");
//         close(fd);
//         return -1;
//     }
//
//     /* Listen for connections */
//     if (listen(fd, SOMAXCONN) == -1) {
//         perror("listen");
//         close(fd);
//         return -1;
//     }
//
//     return fd;   /* success – caller owns this descriptor */
// }

void write_json(struct mg_connection *c, stringa fmt, ...) {
    mg_http_reply(
        c,
        200,
        "Content-Type: application/json; charset=utf-8\r\n",
        "{%m:%lu}\n", MG_ESC("time"), time(NULL));
}

// HTTP server event handler function
void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    static char xbuf[64];
    // if (ev == MG_EV_HTTP_MSG) {
    //     // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    //     // struct mg_http_serve_opts opts = {.root_dir = "./web_root/"};
    //     // mg_http_serve_dir(c, hm, &opts);
    // }
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        snprintf(xbuf, sizeof xbuf, "HTTP request: %%.%llus %%.%llus", hm->method.len, hm->uri.len);
        LOG_DEBUG(xbuf, hm->method.buf, hm->uri.buf);

        if (mg_match(hm->uri, mg_str("/api/time/get"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", "{%m:%lu}\n", MG_ESC("time"), time(NULL));
        } else {
            mg_http_reply(c, 500, "", "{%m:%m}\n", MG_ESC("error"), MG_ESC("Unsupported URI"));
        }
    }
}

#ifdef _WIN32
/* Windows: the callback must match the signature required by SetConsoleCtrlHandler() */
BOOL WINAPI win_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT && user_handler) {
        user_handler(); /* call the user function   */
        return TRUE; /* tell OS we handled it   */
    }
    return FALSE; /* let other handlers run   */
}
#else
/* Unix: use a normal signal handler for SIGINT */
void unix_sigint(int signo) {
    (void) signo; /* unused, but keeps compiler happy */
    if (user_handler) {
        user_handler();
    }
}
#endif

int install_ctrl_c_handler(void (*handler)(void)) {
    if (!handler) return -1; /* nothing to do */
    user_handler = handler;

#ifdef _WIN32
    if (!SetConsoleCtrlHandler(win_ctrl_handler, TRUE)) {
        perror("SetConsoleCtrlHandler");
        return -1;
    }
#else
    struct sigaction sa;
    sa.sa_handler = unix_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* or SA_RESTART if you want */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
#endif
    return 0; /* success */
}

void handle_ctrl_c() {
    LOG_INFO("Requested daemon to be closed");
    _runDaemon = false;
}

int remove_ctrl_c_handler(void) {
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(win_ctrl_handler, FALSE)) {
        perror("SetConsoleCtrlHandler");
        return -1;
    }
#else
    /* Reset to default behaviour */
    signal(SIGINT, SIG_DFL);
#endif
    user_handler = NULL;
    return 0;
}

int loglevel_to_mg_ll(log_level_t logLevel) {
    switch (logLevel) {
        case LOG_LEVEL_DEBUG: return MG_LL_DEBUG;
        case LOG_LEVEL_INFO:
        case LOG_LEVEL_WARNING: return MG_LL_INFO;
        case LOG_LEVEL_ERROR:
        case LOG_LEVEL_FATAL: return MG_LL_ERROR;
    }
}

void mg_log_to_log(char c, void *param) {
    // LOG_
}

static int mg_log_level_ticket = MG_LL_INFO;

void mg_log_prefix(int level, const char *file, int line, const char *fname) {
    mg_log_level_ticket = level;

    // const char *p = strrchr(file, '/');
    // char buf[41];
    // size_t n;
    // if (p == NULL) p = strrchr(file, '\\');
    // n = mg_snprintf(buf, sizeof(buf), "%-6llx %d %s:%d:%s", mg_millis(), level,
    //                 p == NULL ? file : p + 1, line, fname);
    // if (n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    // while (n < sizeof(buf)) buf[n++] = ' ';
    // logs(buf, n - 1);
}

void mg_log(const char *fmt, ...) {
    log_level_t log_level;
    switch (mg_log_level_ticket) {
        case MG_LL_INFO:
            log_level = LOG_LEVEL_INFO;
            break;
        case MG_LL_ERROR:
            log_level = LOG_LEVEL_ERROR;
            break;
        case MG_LL_DEBUG:
        case MG_LL_VERBOSE:
        default:
            log_level = LOG_LEVEL_DEBUG;
    }

    va_list ap;
    va_start(ap, fmt);
    VLOG(log_level, fmt, ap);
    va_end(ap);
}
