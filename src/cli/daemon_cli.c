#include <avar.h>
#include <cli.h>
#include <daemon.h>

#include <stdio.h>
#include <string.h>

static int handle_daemon_start(int argc, char *argv[]);
static int handle_daemon_stop(int argc, char *argv[]);
static int handle_daemon_restart(int argc, char *argv[]);

static void print_daemon_command_help(void) {
    arg_lit_t *attached = arg_lit0(NULL, "attached", "run in foreground");
    arg_str_t *ports = arg_str0(NULL, "ports", "PORTS", "listen ports");
    arg_lit_t *http = arg_lit0(NULL, "http", "enable HTTP");
    arg_lit_t *https = arg_lit0(NULL, "https", "enable HTTPS");
    arg_lit_t *wait = arg_lit0(NULL, "wait", "wait for daemon to stop");
    arg_lit_t *kill = arg_lit0(NULL, "kill", "force kill daemon");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(2);
    void *argtable[] = {attached, ports, http, https, wait, kill, help, end};

    puts("Avar Download Manager - Daemon");
    puts("");
    puts("Usage:");
    puts("  avar daemon start [--attached] [--ports=<ports>] [--http] [--https]");
    puts("  avar daemon stop [--wait] [--kill]");
    puts("  avar daemon restart");
    puts("");
    puts("Options:");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
}

int handle_daemon(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 2, print_daemon_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        fatal("Expected at least 3 parameters");
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

    LOG_ERROR("Unknown sub command '%s' in 'daemon'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_daemon_start(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar daemon start", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *attached = arg_lit0(NULL, "attached", "run in foreground");
    arg_str_t *ports = arg_str0(NULL, "ports", "PORTS", "listen ports");
    arg_lit_t *http = arg_lit0(NULL, "http", "enable HTTP");
    arg_lit_t *https = arg_lit0(NULL, "https", "enable HTTPS");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {attached, ports, http, https, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    (void) attached;
    (void) ports;
    (void) http;
    (void) https;
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return start_daemon();
}

static int handle_daemon_stop(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar daemon stop", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
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

    (void) wait;
    (void) kill;
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return stop_daemon();
}

static int handle_daemon_restart(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar daemon restart", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
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
    return restart_daemon();
}
