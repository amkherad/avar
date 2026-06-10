#include <cJSON.h>

#include <avar.h>
#include <cli.h>
#include <daemon/daemon.h>
#include <daemon/daemon_rpc.h>
#include <daemon/daemon_session.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <signal.h>
    #include <unistd.h>
#endif

static int handle_daemon_start(int argc, char *argv[]);
static int handle_daemon_stop(int argc, char *argv[]);
static int handle_daemon_restart(int argc, char *argv[]);
static int handle_daemon_reload(int argc, char *argv[]);
static int handle_daemon_attach(int argc, char *argv[]);
static int handle_daemon_logs(int argc, char *argv[]);
static int handle_daemon_status(int argc, char *argv[]);
static int handle_daemon_ping(int argc, char *argv[]);
static int handle_daemon_install(int argc, char *argv[]);
static int handle_daemon_uninstall(int argc, char *argv[]);

static void print_daemon_command_help(void) {
    puts("Avar Download Manager - Daemon");
    puts("");
    puts("Usage:");
    puts("  avar daemon start [--attached] [--http] [--pipe] [--unix] [--container]");
    puts("                    [--port=<n>] [--pipeName=<name>] [--unixPath=<path>]");
    puts("                    [--pidFile=<path>] [--no-detach]");
    puts("  avar daemon stop [--wait] [--kill]");
    puts("  avar daemon restart");
    puts("  avar daemon reload");
    puts("  avar daemon attach");
    puts("  avar daemon logs [--follow]");
    puts("  avar daemon status");
    puts("  avar daemon ping");
    puts("  avar daemon install [--systemd|--launchd|--windows-service] [--user]");
    puts("  avar daemon uninstall [--systemd|--launchd|--windows-service] [--user]");
    puts("");
    puts("Session and channels are configured in config.json under 'daemon'.");
    puts("CLI defaults to the pipe channel for local daemon communication.");
}

int handle_daemon(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 2, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        print_daemon_command_help();
        return EXIT_UNKNOWN_COMMAND;
    }

    const char *sub = argv[2];
    if (strcmp(sub, "start") == 0) {
        return handle_daemon_start(argc, argv);
    }
    if (strcmp(sub, "stop") == 0) {
        return handle_daemon_stop(argc, argv);
    }
    if (strcmp(sub, "restart") == 0) {
        return handle_daemon_restart(argc, argv);
    }
    if (strcmp(sub, "reload") == 0) {
        return handle_daemon_reload(argc, argv);
    }
    if (strcmp(sub, "attach") == 0) {
        return handle_daemon_attach(argc, argv);
    }
    if (strcmp(sub, "logs") == 0) {
        return handle_daemon_logs(argc, argv);
    }
    if (strcmp(sub, "status") == 0) {
        return handle_daemon_status(argc, argv);
    }
    if (strcmp(sub, "ping") == 0) {
        return handle_daemon_ping(argc, argv);
    }
    if (strcmp(sub, "install") == 0) {
        return handle_daemon_install(argc, argv);
    }
    if (strcmp(sub, "uninstall") == 0) {
        return handle_daemon_uninstall(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'daemon'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_daemon_start(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon start", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *attached = arg_lit0(NULL, "attached", "run in foreground");
    arg_str_t *port = arg_str0(NULL, "port", "PORT", "HTTP listen port");
    arg_str_t *pipe_name = arg_str0(NULL, "pipeName", "NAME", "named pipe name");
    arg_str_t *unix_path = arg_str0(NULL, "unixPath", "PATH", "unix socket path");
    arg_str_t *pid_file = arg_str0(NULL, "pidFile", "PATH", "pid file path");
    arg_lit_t *http = arg_lit0(NULL, "http", "enable HTTP channel");
    arg_lit_t *pipe = arg_lit0(NULL, "pipe", "enable named-pipe channel");
    arg_lit_t *unix_socket = arg_lit0(NULL, "unix", "enable unix-socket channel");
    arg_lit_t *no_detach = arg_lit0(NULL, "no-detach", "stay in foreground");
    arg_lit_t *container = arg_lit0(NULL, "container", "double-fork for containers");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {attached, port, pipe_name, unix_path, pid_file,
                        http, pipe, unix_socket, no_detach, container, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    DaemonConfig cfg;
    daemon_config_load(&cfg);

    const DaemonStartOptions opts = {
        .foreground = attached->count > 0,
        .http_override = http->count > 0,
        .pipe_override = pipe->count > 0,
        .unix_override = unix_socket->count > 0,
        .no_detach = no_detach->count > 0 || attached->count > 0,
        .container_mode = container->count > 0,
        .http_port = port->count > 0 ? port->sval[0] : NULL,
        .pipe_name = pipe_name->count > 0 ? pipe_name->sval[0] : NULL,
        .unix_path = unix_path->count > 0 ? unix_path->sval[0] : NULL,
        .pid_file = pid_file->count > 0 ? pid_file->sval[0] : NULL,
    };
    daemon_config_apply_start_options(&cfg, &opts);

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    if (attached->count > 0 || no_detach->count > 0) {
        return daemon_start(&cfg);
    }

    if (cfg.server.detach) {
        return daemon_spawn_detached(&cfg);
    }

    return daemon_start(&cfg);
}

static int handle_daemon_stop(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon stop", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *wait = arg_lit0(NULL, "wait", "wait for daemon to stop");
    arg_lit_t *kill = arg_lit0(NULL, "kill", "force kill daemon");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {wait, kill, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const DaemonStopOptions opts = {
        .wait = wait->count > 0,
        .force_kill = kill->count > 0,
    };

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return daemon_stop(&opts);
}

static int handle_daemon_restart(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon restart", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    DaemonConfig cfg;
    daemon_config_load(&cfg);
    return daemon_restart(&cfg);
}

static int handle_daemon_reload(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 3, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    int pid = 0;
    if (!daemon_is_running(&pid)) {
        LOG_ERROR("Daemon is not running");
        return EXIT_FAILURE;
    }

#if defined(_WIN32)
    const DaemonConfig *cfg = daemon_session_config();
    if (cfg == NULL) {
        return EXIT_FAILURE;
    }
    char *result = NULL;
    if (!daemon_rpc_call(cfg->session.transport, cfg, "daemon.reload", "{}", &result, NULL)) {
        LOG_ERROR("Failed to reload daemon configuration");
        free(result);
        return EXIT_FAILURE;
    }
    free(result);
#else
    if (kill((pid_t)pid, SIGHUP) != 0) {
        LOG_ERROR("Failed to send reload signal to daemon (pid %d)", pid);
        return EXIT_FAILURE;
    }
#endif

    LOG_INFO("Daemon reload requested");
    return EXIT_SUCCESS;
}

static int handle_daemon_attach(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 3, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    if (!daemon_session_ping()) {
        LOG_ERROR("Daemon is not reachable; start it with 'avar daemon start'");
        return EXIT_FAILURE;
    }

    printf("Attached to daemon. Press Ctrl+C to detach.\n");

    while (daemon_session_ping()) {
        const DaemonConfig *cfg = daemon_session_config();
        if (cfg == NULL) {
            break;
        }

        char *result = NULL;
        if (daemon_rpc_call(cfg->session.transport, cfg, "health", "{}", &result, NULL) && result != NULL) {
            cJSON *health = cJSON_Parse(result);
            if (health != NULL) {
                const cJSON *queues = cJSON_GetObjectItemCaseSensitive(health, "queueCount");
                const cJSON *active = cJSON_GetObjectItemCaseSensitive(health, "activeDownloads");
                const cJSON *uptime = cJSON_GetObjectItemCaseSensitive(health, "uptimeSeconds");
                printf("\rqueues=%.0f active=%.0f uptime=%.0fs   ",
                       cJSON_IsNumber(queues) ? queues->valuedouble : 0.0,
                       cJSON_IsNumber(active) ? active->valuedouble : 0.0,
                       cJSON_IsNumber(uptime) ? uptime->valuedouble : 0.0);
                fflush(stdout);
                cJSON_Delete(health);
            }
            free(result);
        }

#if defined(_WIN32)
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    printf("\n");
    LOG_INFO("Daemon stopped; detaching");
    return EXIT_SUCCESS;
}

static int handle_daemon_logs(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon logs", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *follow = arg_lit0(NULL, "follow", "stream new log lines");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {follow, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const bool want_follow = follow->count > 0;
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    if (!daemon_session_ping()) {
        LOG_ERROR("Daemon is not reachable");
        return EXIT_FAILURE;
    }

    const DaemonConfig *cfg = daemon_session_config();
    if (cfg == NULL) {
        return EXIT_FAILURE;
    }

    do {
        char *result = NULL;
        const char *params = want_follow ? "{\"follow\":true,\"maxLines\":100}" : "{\"maxLines\":100}";
        if (!daemon_rpc_call(cfg->session.transport, cfg, "logs.get", params, &result, NULL)) {
            LOG_ERROR("Failed to fetch daemon logs");
            free(result);
            return EXIT_FAILURE;
        }

        if (result != NULL) {
            cJSON *parsed = cJSON_Parse(result);
            if (parsed != NULL) {
                const cJSON *logs = cJSON_GetObjectItemCaseSensitive(parsed, "logs");
                if (cJSON_IsString(logs) && logs->valuestring != NULL) {
                    fputs(logs->valuestring, stdout);
                }
                cJSON_Delete(parsed);
            }
            free(result);
        }

        if (!want_follow) {
            break;
        }

#if defined(_WIN32)
        Sleep(1000);
#else
        sleep(1);
#endif
    } while (daemon_session_ping());

    return EXIT_SUCCESS;
}

static int handle_daemon_status(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 3, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    DaemonConfig cfg;
    daemon_config_load(&cfg);

    int pid = 0;
    const bool running = daemon_is_running(&pid);

    printf("daemon.running: %s\n", running ? "true" : "false");
    if (running) {
        printf("daemon.pid: %d\n", pid);
    }
    printf("daemon.reachable: %s\n", daemon_session_ping() ? "true" : "false");
    printf("daemon.session.mode: %s\n",
           cfg.session.mode == AvarSessionModeRemote ? AVAR_DAEMON_SESSION_MODE_REMOTE
                                                   : AVAR_DAEMON_SESSION_MODE_LOCAL);
    printf("daemon.session.transport: %s\n",
           cfg.session.transport == AvarTransportPipe    ? AVAR_DAEMON_TRANSPORT_PIPE
           : cfg.session.transport == AvarTransportUnix ? AVAR_DAEMON_TRANSPORT_UNIX
           : cfg.session.transport == AvarTransportHttp ? AVAR_DAEMON_TRANSPORT_HTTP
                                                          : AVAR_DAEMON_TRANSPORT_LOCAL);
    printf("daemon.server.channels.http.enabled: %s\n", cfg.server.http.enabled ? "true" : "false");
    printf("daemon.server.channels.http.port: %u\n", (unsigned)cfg.server.http.port);
    printf("daemon.server.channels.https.enabled: %s\n", cfg.server.https.enabled ? "true" : "false");
    printf("daemon.server.channels.pipe.enabled: %s\n", cfg.server.pipe.enabled ? "true" : "false");
    printf("daemon.server.channels.pipe.name: %s\n", cfg.server.pipe.name);
    printf("daemon.server.channels.unix.enabled: %s\n", cfg.server.unix_socket.enabled ? "true" : "false");
    printf("daemon.server.containerMode: %s\n", cfg.server.container_mode ? "true" : "false");
    printf("daemon.server.pidFile: %s\n", cfg.server.pid_file);

    return EXIT_SUCCESS;
}

static int handle_daemon_ping(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 3, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    if (daemon_session_ping()) {
        puts("pong");
        return EXIT_SUCCESS;
    }

    LOG_ERROR("Daemon did not respond");
    return EXIT_FAILURE;
}

static int handle_daemon_install(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon install", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *systemd = arg_lit0(NULL, "systemd", "install systemd unit");
    arg_lit_t *launchd = arg_lit0(NULL, "launchd", "install launchd plist");
    arg_lit_t *windows_service = arg_lit0(NULL, "windows-service", "install Windows service script");
    arg_lit_t *user = arg_lit0(NULL, "user", "install user-level unit");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {systemd, launchd, windows_service, user, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const bool want_systemd = systemd->count > 0;
    const bool want_launchd = launchd->count > 0;
    const bool want_windows = windows_service->count > 0;
    const bool user_unit = user->count > 0;
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    char path[AVAR_CONFIG_PATH_MAX] = {0};
    if (want_systemd) {
        return daemon_install_systemd(user_unit, path, sizeof path) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (want_launchd) {
        return daemon_install_launchd(user_unit, path, sizeof path) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (want_windows) {
        return daemon_install_windows_service(path, sizeof path) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    LOG_ERROR("Specify --systemd, --launchd, or --windows-service");
    return EXIT_FAILURE;
}

static int handle_daemon_uninstall(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar daemon uninstall", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *systemd = arg_lit0(NULL, "systemd", "remove systemd unit");
    arg_lit_t *launchd = arg_lit0(NULL, "launchd", "remove launchd plist");
    arg_lit_t *windows_service = arg_lit0(NULL, "windows-service", "remove Windows service script");
    arg_lit_t *user = arg_lit0(NULL, "user", "remove user-level unit");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {systemd, launchd, windows_service, user, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const bool want_systemd = systemd->count > 0;
    const bool want_launchd = launchd->count > 0;
    const bool want_windows = windows_service->count > 0;
    const bool user_unit = user->count > 0;
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    if (want_systemd) {
        return daemon_uninstall_systemd(user_unit) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (want_launchd) {
        return daemon_uninstall_launchd(user_unit) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (want_windows) {
        return daemon_uninstall_windows_service() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    LOG_ERROR("Specify --systemd, --launchd, or --windows-service");
    return EXIT_FAILURE;
}
