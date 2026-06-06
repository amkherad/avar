#include <cli.h>
#include <avar.h>
#include <download.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Command commands[] = {
    {"download", handle_download},
    {"dl", handle_download},
    {"profile", handle_profile},
    {"queue", handle_queue},
    {"scheduler", handle_scheduler},
    {"config", handle_config},
    {"daemon", handle_daemon},
};

static bool is_verbose(int argc, char *argv[]);
static int remove_argv_entry(int *p_argc, char ***pp_argv, const char *to_remove);

int cli_run(int argc, char *argv[]) {
    const bool verbose = is_verbose(argc, argv);
    init_logger(verbose);
    if (verbose) {
        remove_argv_entry(&argc, &argv, "--verbose");
    }

    LOG_DEBUG(APP_NAME " was called with %d parameters", argc);

    if (argc < 2) {
        cli_print_avar_help();
        return EXIT_SUCCESS;
    }

    const char *cmd = argv[1];
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            LOG_DEBUG("Found a handler for command '%s'", commands[i].name);
            return commands[i].handler(argc, argv);
        }
    }

    AvarArgs args;
    const int parse_rc = cli_parse_avar(argc, argv, &args);
    if (parse_rc != EXIT_SUCCESS) {
        return parse_rc;
    }

    if (args.help) {
        return EXIT_SUCCESS;
    }

    if (args.version) {
        printf("Avar version: %s\n", VERSION_STR);
        return EXIT_SUCCESS;
    }

    if (args.url != NULL && is_valid_http_url(args.url)) {
        LOG_DEBUG("Found a valid url '%s'", args.url);
        return transient_download(args.url, args.queue, args.name, args.detached);
    }

    LOG_ERROR("Unknown command '%s'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}

static bool is_verbose(const int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            return true;
        }
    }

    return false;
}

static int remove_argv_entry(int *p_argc, char ***pp_argv, const char *to_remove) {
    int argc = *p_argc;
    char **argv = *pp_argv;

    int i;
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], to_remove) == 0)
            break;
    }

    if (i == argc) {
        return -1;
    }

    memmove(&argv[i], &argv[i + 1],
            sizeof(char *) * (argc - i));

    argv[argc - 1] = NULL;

    (*p_argc)--;
    *pp_argv = argv;

    return 0;
}
