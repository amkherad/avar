#include <cJSON.h>

#include <avar.h>
#include <cli.h>
#include <config.h>
#include <daemon/daemon.h>
#include <daemon/daemon_rpc.h>
#include <daemon/daemon_session.h>
#include <utils.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <io.h>
    #include <windows.h>
#else
    #include <signal.h>
    #include <time.h>
    #include <unistd.h>
#endif

#define ATTACH_SPEED_TRACK_MAX 32U
#define ATTACH_PROGRESS_LABEL_MAX 48U

typedef struct {
    char id[AVAR_DL_ID_BUF_SIZE];
    uint64_t last_bytes;
    uint64_t last_ms;
    double speed_bps;
} AttachSpeedSample;

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

static bool attach_stdout_is_tty(void) {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO);
#endif
}

static uint64_t attach_now_ms(void) {
#if defined(_WIN32)
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
#endif
}

static void attach_enable_vt_if_needed(void) {
#if defined(_WIN32)
    static bool enabled = false;
    if (enabled || !attach_stdout_is_tty()) {
        return;
    }
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    enabled = true;
#endif
}

static void attach_cursor_up(const size_t lines) {
    if (lines == 0U || !attach_stdout_is_tty()) {
        return;
    }
    attach_enable_vt_if_needed();
    printf("\033[%zuA", lines);
}

static void attach_clear_line(void) {
    if (!attach_stdout_is_tty()) {
        return;
    }
    attach_enable_vt_if_needed();
    fputs("\033[2K\r", stdout);
}

static double attach_speed_bps(AttachSpeedSample *samples, size_t *sample_count, const char *id,
                               const uint64_t bytes) {
    if (id == NULL || id[0] == '\0') {
        return 0.0;
    }

    const uint64_t now = attach_now_ms();
    for (size_t i = 0; i < *sample_count; ++i) {
        if (strcmp(samples[i].id, id) != 0) {
            continue;
        }

        if (samples[i].last_ms > 0U && now > samples[i].last_ms) {
            const double elapsed = (double)(now - samples[i].last_ms) / 1000.0;
            if (elapsed >= 0.2) {
                samples[i].speed_bps = (double)(bytes - samples[i].last_bytes) / elapsed;
                samples[i].last_bytes = bytes;
                samples[i].last_ms = now;
            }
        } else if (samples[i].last_ms == 0U) {
            samples[i].last_bytes = bytes;
            samples[i].last_ms = now;
        }
        return samples[i].speed_bps;
    }

    if (*sample_count >= ATTACH_SPEED_TRACK_MAX) {
        return 0.0;
    }

    AttachSpeedSample *slot = &samples[*sample_count];
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->id, sizeof slot->id, "%s", id);
    slot->last_bytes = bytes;
    slot->last_ms = now;
    (*sample_count)++;
    return 0.0;
}

static void attach_format_label(const char *filename, char *label, const size_t label_size) {
    if (label_size == 0U) {
        return;
    }

    if (filename == NULL || filename[0] == '\0') {
        snprintf(label, label_size, "(download)");
        return;
    }

    if (strlen(filename) + 1U <= label_size) {
        snprintf(label, label_size, "%s", filename);
        return;
    }

    if (label_size <= 4U) {
        snprintf(label, label_size, "%s", filename);
        return;
    }

    snprintf(label, label_size, "...%s", filename + strlen(filename) - (label_size - 4U));
}

static void attach_print_progress_line(const char *filename, const uint64_t bytes_downloaded,
                                       const uint64_t total_bytes, const double speed_bps) {
    AvarSizeUnit size_unit = AVAR_SIZE_MIB;
    AvarSpeedUnit speed_unit = AVAR_SPEED_MIBIT_PER_SEC;
    char *size_cfg = get_config_or_default(AVAR_CFG_DM_PROGRESS_SIZE_UNIT, AVAR_DEFAULT_SIZE_UNIT);
    char *speed_cfg =
        get_config_or_default(AVAR_CFG_DM_PROGRESS_SPEED_UNIT, AVAR_DEFAULT_SPEED_UNIT);
    (void)avar_size_unit_parse(size_cfg, &size_unit);
    (void)avar_speed_unit_parse(speed_cfg, &speed_unit);
    free(size_cfg);
    free(speed_cfg);

    const int percent = avar_progress_percent(bytes_downloaded, total_bytes);

    char label[ATTACH_PROGRESS_LABEL_MAX];
    attach_format_label(filename, label, sizeof label);

    char percent_str[DL_PROGRESS_PERCENT_WIDTH + 2];
    char done_str[32];
    char total_str[32];
    char speed_str[32];
    char suffix[192];
    char line[DL_PROGRESS_LINE_BUF_SIZE];
    const bool has_total = total_bytes > 0U;
    const int total_num_width = has_total ? avar_data_size_number_width(total_bytes, size_unit) : 0;
    const int done_num_width = avar_data_size_number_width(bytes_downloaded, size_unit);

    format_progress_percent(percent, percent_str, sizeof percent_str);
    format_data_size_padded(bytes_downloaded, size_unit, done_num_width, done_str, sizeof done_str);
    format_transfer_rate_padded(speed_bps, speed_unit, DL_PROGRESS_SPEED_NUMBER_WIDTH, speed_str,
                                sizeof speed_str);

    if (has_total) {
        format_data_size_padded(total_bytes, size_unit, total_num_width, total_str, sizeof total_str);
        snprintf(suffix, sizeof suffix, " %s, %s, (%s / %s)", percent_str, speed_str, done_str,
                 total_str);
    } else {
        snprintf(suffix, sizeof suffix, " %s, (%s)", speed_str, done_str);
    }

    const int label_width = (int)strlen(label);
    const int term_cols = avar_terminal_columns();
    int bar_width = avar_progress_bar_width((int)strlen(suffix) + label_width + 1);
    if (term_cols > 0) {
        bar_width = avar_progress_bar_width_for_columns(term_cols, (int)strlen(suffix) + label_width + 1);
    }

    char bar[DL_PROGRESS_BAR_WIDTH_MAX + 3];
    if (has_total) {
        format_progress_bar_bytes(bytes_downloaded, total_bytes, bar_width, bar, sizeof bar);
    } else {
        format_progress_bar(percent, bar_width, bar, sizeof bar);
    }
    snprintf(line, sizeof line, "%s %s%s", label, bar, suffix);

    attach_clear_line();
    fputs(line, stdout);
    fputc('\n', stdout);
}

static void attach_render_health(const cJSON *health, AttachSpeedSample *samples,
                                 size_t *sample_count, size_t *last_line_count) {
    const cJSON *queues = cJSON_GetObjectItemCaseSensitive(health, "queueCount");
    const cJSON *active = cJSON_GetObjectItemCaseSensitive(health, "activeDownloads");
    const cJSON *uptime = cJSON_GetObjectItemCaseSensitive(health, "uptimeSeconds");
    const cJSON *downloads = cJSON_GetObjectItemCaseSensitive(health, "downloads");

    if (*last_line_count > 0U) {
        attach_cursor_up(*last_line_count);
    }

    char status_line[128];
    snprintf(status_line, sizeof status_line, "queues=%.0f active=%.0f uptime=%.0fs",
             cJSON_IsNumber(queues) ? queues->valuedouble : 0.0,
             cJSON_IsNumber(active) ? active->valuedouble : 0.0,
             cJSON_IsNumber(uptime) ? uptime->valuedouble : 0.0);
    attach_clear_line();
    fputs(status_line, stdout);
    fputc('\n', stdout);

    size_t lines = 1U;
    if (cJSON_IsArray(downloads)) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, downloads) {
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_ID);
            const cJSON *filename = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_FILENAME);
            const cJSON *done = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_BYTES_DOWNLOADED);
            const cJSON *total = cJSON_GetObjectItemCaseSensitive(entry, AVAR_FIELD_TOTAL_BYTES);

            const char *id_str = cJSON_IsString(id) ? id->valuestring : "";
            const char *filename_str = cJSON_IsString(filename) ? filename->valuestring : "";
            const uint64_t bytes_downloaded =
                cJSON_IsNumber(done) ? (uint64_t)done->valuedouble : 0U;
            const uint64_t total_bytes =
                cJSON_IsNumber(total) ? (uint64_t)total->valuedouble : 0U;
            const double speed_bps =
                attach_speed_bps(samples, sample_count, id_str, bytes_downloaded);

            attach_print_progress_line(filename_str, bytes_downloaded, total_bytes, speed_bps);
            lines++;
        }
    }

    *last_line_count = lines;
    fflush(stdout);
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

    AttachSpeedSample speed_samples[ATTACH_SPEED_TRACK_MAX];
    size_t speed_sample_count = 0U;
    size_t last_line_count = 0U;

    while (daemon_session_ping()) {
        const DaemonConfig *cfg = daemon_session_config();
        if (cfg == NULL) {
            break;
        }

        char *result = NULL;
        if (daemon_rpc_call(cfg->session.transport, cfg, "health", "{}", &result, NULL) &&
            result != NULL) {
            cJSON *health = cJSON_Parse(result);
            if (health != NULL) {
                attach_render_health(health, speed_samples, &speed_sample_count, &last_line_count);
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

    if (last_line_count > 0U) {
        attach_cursor_up(last_line_count);
        for (size_t i = 0; i < last_line_count; ++i) {
            attach_clear_line();
            fputc('\n', stdout);
        }
        attach_cursor_up(last_line_count);
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
