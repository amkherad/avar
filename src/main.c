#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avar.h>
#include <cli.h>
#include <download.h>
#include <profile.h>
#include <queue.h>
#include <daemon.h>

#ifdef _WIN32
#include <windows.h>
#endif

static Command commands[] = {
    {"download", handle_download},
    {"dl", handle_download},
    {"profile", handle_profile},
    {"queue", handle_queue},
    {"scheduler", handle_scheduler},
    {"config", handle_config},
    {"batch", handle_batch},
    {"daemon", handle_daemon},
};

bool _is_verbose(int argc, char *argv[]);
int remove_argv_entry(int *p_argc, char ***pp_argv, const char *to_remove);

int main(int argc, char *argv[]) {
    auto isVerbose = _is_verbose(argc, argv);
    init_logger(isVerbose);
    if (isVerbose) {
        remove_argv_entry(&argc, &argv, "--verbose");
    }

    LOG_DEBUG(APP_NAME " was called with %d parameters", argc);

    struct avar_DocoptArgs args = parse_avar_docopt(argc, argv, /* help */ 0, /* version */ VERSION_STR);

    LOG_DEBUG("Successfully parsed parameters");

    const char *cmd = argv[1];
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            LOG_DEBUG("Found a handler for command '%s'", commands[i].name);

            // Call the handler with shifted args
            return commands[i].handler(argc, argv);
        }
    }

    if (args.help) {
        return print_help(args.help_message_n, args.help_message);
    }

    if (is_valid_http_url(cmd)) {
        LOG_DEBUG("Found a valid url '%s'", cmd);

        return transient_download();
    }

    LOG_DEBUG("No handler was found for command '%s'", cmd);

    if (args.version) {
        printf("Avar version: %p\n", VERSION_STR);
        return EXIT_SUCCESS;
    }

    LOG_ERROR("Unknown command '%s'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}

int handle_batch(int argc, char *argv[]) {
    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    struct batch_DocoptArgs args = parse_batch_docopt(argc, argv, /* help */ 1, /* version */ VERSION_STR);

    const char *cmd = argv[2];

    LOG_ERROR("Unknown sub command '%s' in 'batch'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}

inline bool _is_verbose(const int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            return true;
        }
    }

    return false;
}

inline int remove_argv_entry(int *p_argc, char ***pp_argv, const char *to_remove) {
    int argc = *p_argc;
    char **argv = *pp_argv;

    /* Find the first matching entry.  Compare by string equality. */
    int i;
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], to_remove) == 0)
            break;
    }

    if (i == argc) {
        // not found
        return -1;
    }

    /* Shift everything after `i` left by one position. */
    memmove(&argv[i], &argv[i + 1],
            sizeof(char *) * (argc - i));

    /* argv[argc-1] is now a duplicate of the NULL terminator; set it explicitly. */
    argv[argc - 1] = NULL;

    /* Update argc and optionally return the new argv pointer. */
    (*p_argc)--;
    *pp_argv = argv;

    return 0;
}
