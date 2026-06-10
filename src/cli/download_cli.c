#include <avar.h>
#include <cli.h>
#include <daemon_session.h>
#include <download.h>

#include <stdio.h>
#include <string.h>

static int handle_download_url(int argc, char *argv[]);
static int handle_download_add(int argc, char *argv[]);
static int handle_download_rm(int argc, char *argv[]);
static int handle_download_ls(int argc, char *argv[]);

static void print_download_command_help(void) {
    arg_str_t *url = arg_str0(NULL, NULL, "URL", "download URL");
    arg_str_t *queue = arg_str0(NULL, "queue", "QUEUE", "target queue name");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(2);
    void *argtable[] = {url, queue, help, end};

    puts("Avar Download Manager - Download");
    puts("");
    puts("Usage:");
    puts("  avar dl|download <url> [--attached] [--queue=<queue>]");
    puts("      (default: queue on daemon; falls back to --attached if daemon is down)");
    puts("  avar dl add <name>");
    puts("  avar dl rm <name> [--force]");
    puts("  avar dl ls [--list]");
    puts("");
    puts("Options:");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
}

int handle_download(int argc, char *argv[]) {
    if (argc < 2) {
        fatal("Expected at least 2 parameters");
    }

    if (daemon_session_is_remote()) {
        return daemon_session_delegate_argv(argc, argv);
    }

    if (cli_try_command_help(argc, argv, 2, print_download_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc >= 3 && is_valid_http_url(argv[2])) {
        return handle_download_url(argc, argv);
    }

    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    const char *sub = argv[2];
    if (strcmp(sub, "add") == 0) {
        return handle_download_add(argc, argv);
    }
    if (strcmp(sub, "rm") == 0) {
        return handle_download_rm(argc, argv);
    }
    if (strcmp(sub, "ls") == 0) {
        return handle_download_ls(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'download'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_download_url(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar dl", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *url = arg_str1(NULL, NULL, "URL", "download URL");
    arg_str_t *queue = arg_str0(NULL, "queue", "QUEUE", "target queue name");
    arg_lit_t *attached = arg_lit0(NULL, "attached", "run download in foreground with progress");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {url, queue, attached, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }
    if (help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_SUCCESS;
    }

    const bool is_attached = attached->count > 0;
    const int rc = daemon_session_download_url(url->sval[0],
                                               queue->count > 0 ? queue->sval[0] : NULL, NULL,
                                               is_attached);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return rc;
}

static int handle_download_add(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar dl add", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "download name");
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

    printf("download add: %s\n", name->sval[0]);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_download_rm(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar dl rm", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "download name");
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

    printf("download rm: %s%s\n", name->sval[0], force->count > 0 ? " (force)" : "");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_download_ls(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 2, "avar dl ls", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *list = arg_lit0(NULL, "list", "list downloads");
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

    puts("download ls");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
