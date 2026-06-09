#include <cli.h>
#include <avar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cli_run_argtable(const char *progname, void **argtable, arg_end_t *end, int argc, char **argv, bool *help_out) {
    if (help_out != NULL) {
        *help_out = false;
    }

    const int nullcheck = arg_nullcheck(argtable);
    if (nullcheck != 0) {
        fprintf(stderr, "%s: argtable allocation failed (error %d)\n", progname, nullcheck);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        if (cli_is_help_flag(argv[i])) {
            cli_print_argtable_help(progname, argtable);
            if (help_out != NULL) {
                *help_out = true;
            }
            return EXIT_SUCCESS;
        }
    }

    const int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors > 0) {
        arg_print_errors(stderr, end, progname);
        fprintf(stderr, "Try '%s --help' for more information.\n", progname);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int cli_make_subargv(int argc, char **argv, int skip, const char *progname, int *sub_argc, char ***sub_argv) {
    if (skip >= argc) {
        *sub_argc = 1;
        char **av = calloc(2, sizeof(char *));
        if (av == NULL) {
            return EXIT_FAILURE;
        }
        av[0] = (char *) progname;
        av[1] = NULL;
        *sub_argv = av;
        return EXIT_SUCCESS;
    }

    const int count = argc - skip + 1;
    char **av = calloc((size_t) count + 1, sizeof(char *));
    if (av == NULL) {
        return EXIT_FAILURE;
    }

    av[0] = (char *) progname;
    for (int i = skip; i < argc; i++) {
        av[i - skip + 1] = argv[i];
    }
    av[count] = NULL;
    *sub_argc = count;
    *sub_argv = av;
    return EXIT_SUCCESS;
}

void cli_free_subargv(char **sub_argv) {
    free(sub_argv);
}

bool cli_is_help_flag(const char *arg) {
    return arg != NULL && (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0);
}

void cli_print_argtable_help(const char *progname, void **argtable) {
    arg_print_syntaxv(stdout, argtable, "\n");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    (void) progname;
}

bool cli_try_command_help(int argc, char **argv, int help_index, void (*print_help)(void)) {
    if (argc > help_index && cli_is_help_flag(argv[help_index]) && print_help != NULL) {
        print_help();
        return true;
    }
    return false;
}

void cli_print_avar_help(void) {
    printf("%s (v%s)\n", APP_NAME, VERSION_STR);
    puts("Avar Download Manager");
    puts("");
    puts("Usage:");
    puts("  avar <url> [--attached] [--queue=<queue>] [--name=<name>] [--detached]");
    puts("  avar dl|download <url> [--queue=<queue>]");
    puts("  avar queue add <name>");
    puts("  avar queue rm <name> [--force]");
    puts("  avar queue ls");
    puts("  avar config get <name> [--format=<fmt>] [--defaultValue=<v>]");
    puts("  avar config set <name> <value>");
    puts("  avar config reset <name>");
    puts("  avar config reset-all");
    puts("  avar config save <path>");
    puts("  avar config load <path>");
    puts("  avar profile add|rm|ls ...");
    puts("  avar scheduler add|rm|ls ...");
    puts("  avar daemon start|stop|restart ...");
    puts("");
    puts("Options:");
    puts("  -h, --help        Show this help.");
    puts("  -v, --version     Show version.");
    puts("  --verbose         Set log severity to verbose.");
}

int cli_parse_avar(int argc, char **argv, AvarArgs *out) {
    if (out == NULL) {
        return EXIT_FAILURE;
    }

    *out = (AvarArgs) {0};

    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_lit_t *version = arg_lit0("v", "version", "show version");
    arg_str_t *queue = arg_str0(NULL, "queue", "QUEUE", "target queue name");
    arg_str_t *name = arg_str0(NULL, "name", "NAME", "download item name");
    arg_lit_t *detached = arg_lit0("d", "detached", "detach process and return immediately");
    arg_lit_t *attached = arg_lit0(NULL, "attached", "run download in foreground with progress");
    arg_str_t *url = arg_str0(NULL, NULL, "URL", "download URL");
    arg_end_t *end = arg_end(20);

    void *argtable[] = {help, version, queue, name, detached, attached, url, end};

    const int parse_rc = cli_run_argtable(argv[0], argtable, end, argc, argv, NULL);
    if (parse_rc != EXIT_SUCCESS) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (help->count > 0) {
        cli_print_avar_help();
        out->help = true;
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_SUCCESS;
    }

    if (version->count > 0) {
        out->version = true;
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_SUCCESS;
    }

    if (url->count > 0) {
        out->url = url->sval[0];
    } else if (argc > 1 && argv[1][0] != '-') {
        out->url = argv[1];
    }

    if (queue->count > 0) {
        out->queue = queue->sval[0];
    }
    if (name->count > 0) {
        out->name = name->sval[0];
    }
    if (detached->count > 0) {
        out->detached = true;
    }
    if (attached->count > 0) {
        out->attached = true;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
