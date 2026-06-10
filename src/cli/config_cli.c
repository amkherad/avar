#include <cJSON.h>

#include <avar.h>
#include <cli.h>
#include <config.h>
#include <daemon_session.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int handle_config_get(int argc, char *argv[]);
static int handle_config_set(int argc, char *argv[]);
static int handle_config_reset(int argc, char *argv[]);
static int handle_config_reset_all(int argc, char *argv[]);
static int handle_config_save(int argc, char *argv[]);
static int handle_config_load(int argc, char *argv[]);

static void print_config_value(const char *value, const char *format) {
    if (format != NULL && strcmp(format, "json") == 0) {
        cJSON *json = cJSON_CreateString(value);
        if (json == NULL) {
            fatal("Failed to format config value as JSON");
        }

        char *printed = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
        if (printed == NULL) {
            fatal("Failed to format config value as JSON");
        }

        puts(printed);
        cJSON_free(printed);
        return;
    }

    puts(value);
}

static void print_config_command_help(void) {
    arg_str_t *format = arg_str0(NULL, "format", "FMT", "output format (e.g. json)");
    arg_str_t *default_value = arg_str0(NULL, "defaultValue", "VALUE", "default value if key is missing");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(2);
    void *argtable[] = {format, default_value, help, end};

    puts("Avar Download Manager - Config");
    puts("");
    puts("Usage:");
    puts("  avar config get <name> [--format=<fmt>] [--defaultValue=<v>]");
    puts("  avar config set <name> <value>");
    puts("  avar config reset <name>");
    puts("  avar config reset-all");
    puts("  avar config save <path>");
    puts("  avar config load <path>");
    puts("");
    puts("Options:");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
}

int handle_config(int argc, char *argv[]) {
    if (daemon_session_is_remote()) {
        return daemon_session_delegate_argv(argc, argv);
    }

    if (cli_try_command_help(argc, argv, 2, print_config_command_help)) {
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    const char *sub = argv[2];
    if (strcmp(sub, "get") == 0) {
        return handle_config_get(argc, argv);
    }
    if (strcmp(sub, "set") == 0) {
        return handle_config_set(argc, argv);
    }
    if (strcmp(sub, "reset") == 0) {
        return handle_config_reset(argc, argv);
    }
    if (strcmp(sub, "reset-all") == 0) {
        return handle_config_reset_all(argc, argv);
    }
    if (strcmp(sub, "save") == 0) {
        return handle_config_save(argc, argv);
    }
    if (strcmp(sub, "load") == 0) {
        return handle_config_load(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'config'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int handle_config_get(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config get", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "config key");
    arg_str_t *format = arg_str0(NULL, "format", "FMT", "output format (e.g. json)");
    arg_str_t *default_value = arg_str0(NULL, "defaultValue", "VALUE", "default value if key is missing");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {name, format, default_value, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const char *default_arg = default_value->count > 0 ? default_value->sval[0] : NULL;
    char *value = get_config_or_default(name->sval[0], default_arg);
    if (value == NULL) {
        LOG_ERROR("Config key not found: %s", name->sval[0]);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    const char *format_arg = format->count > 0 ? format->sval[0] : NULL;
    print_config_value(value, format_arg);
    free(value);

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_config_set(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config set", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "config key");
    arg_str_t *value = arg_str1(NULL, NULL, "VALUE", "config value");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {name, value, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (set_config(name->sval[0], value->sval[0]) != 0) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_config_reset(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config reset", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "config key");
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

    if (reset_config(name->sval[0]) != 0) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_config_reset_all(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config reset-all", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
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

    if (reset_all_config() != 0) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_config_save(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config save", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *path = arg_str1(NULL, NULL, "PATH", "destination path");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {path, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (save_config(path->sval[0]) != 0) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}

static int handle_config_load(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar config load", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *path = arg_str1(NULL, NULL, "PATH", "source path");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {path, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv, &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (load_config(path->sval[0]) != 0) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
