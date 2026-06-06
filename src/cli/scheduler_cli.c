#include <avar.h>
#include <cli.h>

#include <stdio.h>
#include <string.h>

static int handle_scheduler_add(int argc, char *argv[]);
static int handle_scheduler_rm(int argc, char *argv[]);
static int handle_scheduler_ls(int argc, char *argv[]);

static void print_scheduler_command_help(void) {
    arg_lit_t *force = arg_lit0(NULL, "force", "force removal");
    arg_lit_t *list = arg_lit0(NULL, "list", "list schedulers");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(2);
    void *argtable[] = {force, list, help, end};

    puts("Avar Download Manager - Scheduler");
    puts("");
    puts("Usage:");
    puts("  avar scheduler add <name>");
    puts("  avar scheduler rm <name> [--force]");
    puts("  avar scheduler ls [--list]");
    puts("");
    puts("Options:");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
}

int handle_scheduler(int argc, char *argv[]) {
    if (cli_try_command_help(argc, argv, 2, print_scheduler_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    const char *sub = argv[2];
    if (strcmp(sub, "add") == 0) {
        return handle_scheduler_add(argc, argv);
    }
    if (strcmp(sub, "rm") == 0) {
        return handle_scheduler_rm(argc, argv);
    }
    if (strcmp(sub, "ls") == 0) {
        return handle_scheduler_ls(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'scheduler'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_scheduler_add(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar scheduler add", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "scheduler name");
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

    printf("scheduler add: %s\n", name->sval[0]);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_scheduler_rm(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar scheduler rm", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "scheduler name");
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

    printf("scheduler rm: %s%s\n", name->sval[0], force->count > 0 ? " (force)" : "");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_scheduler_ls(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar scheduler ls", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *list = arg_lit0(NULL, "list", "list schedulers");
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

    puts("scheduler ls");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
