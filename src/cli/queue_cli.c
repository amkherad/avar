#include <avar.h>
#include <cli.h>

#include <stdio.h>
#include <string.h>

static int handle_queue_add(int argc, char *argv[]);
static int handle_queue_rm(int argc, char *argv[]);
static int handle_queue_ls(int argc, char *argv[]);

static void print_queue_command_help(void) {
    arg_lit_t *force = arg_lit0(NULL, "force", "force removal");
    arg_lit_t *list = arg_lit0(NULL, "list", "list queues");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(2);
    void *argtable[] = {force, list, help, end};

    puts("Avar Download Manager - Queue");
    puts("");
    puts("Usage:");
    puts("  avar queue add <name>");
    puts("  avar queue rm <name> [--force]");
    puts("  avar queue ls [--list]");
    puts("");
    puts("Options:");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
}

int handle_queue(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 2, print_queue_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    const char *sub = argv[2];
    if (strcmp(sub, "add") == 0) {
        return handle_queue_add(argc, argv);
    }
    if (strcmp(sub, "rm") == 0) {
        return handle_queue_rm(argc, argv);
    }
    if (strcmp(sub, "ls") == 0) {
        return handle_queue_ls(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'queue'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_queue_add(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue add", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "queue name");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {name, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (name->count == 0 || name->sval[0] == NULL) {
        fatal("Provide a queue name");
    }

    printf("queue add: %s\n", name->sval[0]);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_queue_rm(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue rm", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "queue name");
    arg_lit_t *force = arg_lit0(NULL, "force", "force removal");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {name, force, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    printf("queue rm: %s%s\n", name->sval[0], force->count > 0 ? " (force)" : "");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_queue_ls(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar queue ls", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *list = arg_lit0(NULL, "list", "list queues");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {list, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    puts("queue ls");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
