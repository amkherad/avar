#include <avar.h>
#include <cli.h>
#include <config.h>
#include <daemon/daemon_session.h>
#include <queue.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int handle_queue_add(int argc, char *argv[]);
static int handle_queue_rm(int argc, char *argv[]);
static int handle_queue_edit(int argc, char *argv[]);
static int handle_queue_start(int argc, char *argv[]);
static int handle_queue_stop(int argc, char *argv[]);
static int handle_queue_ls(int argc, char *argv[]);

static int queue_report_error(QueueError error);
static bool parse_uint_arg(const char *text, uint32_t *out);
static void fill_queue_options(const QueueOptions *src, QueueOptions *dst);
static void fill_queue_patch(const QueuePatch *src, QueuePatch *dst);

static void print_queue_command_help(void) {
    puts("Avar Download Manager - Queue");
    puts("");
    puts("Usage:");
    puts("  avar queue add <name> [--description=<text>] [--maxConcurrentDownloads=<n>]");
    puts("                      [--maxConnections=<n>] [--tempPath=<path>]");
    puts("                      [--downloadPath=<path>]");
    puts("  avar queue rm <id> [--name=<name>] [--purge-items|--remove-items]");
    puts("  avar queue edit <id> [--description=<text>] [--maxConcurrentDownloads=<n>]");
    puts("                       [--maxConnections=<n>] [--tempPath=<path>]");
    puts("                       [--downloadPath=<path>]");
    puts("  avar queue start <id> [--name=<name>]");
    puts("  avar queue stop <id> [--name=<name>]");
    puts("  avar queue ls");
    puts("");
    puts("Options:");
    puts("  --name=<name>         Resolve queue by name instead of id");
    puts("  --purge-items         Remove download items when deleting a queue");
    puts("  --remove-items        Alias for --purge-items");
    puts("  -h, --help            Show help");
}

int handle_queue(int argc, char *argv[]) {
    if (daemon_session_is_remote()) {
        return daemon_session_delegate_argv(argc, argv);
    }

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
    if (strcmp(sub, "edit") == 0) {
        return handle_queue_edit(argc, argv);
    }
    if (strcmp(sub, "start") == 0) {
        return handle_queue_start(argc, argv);
    }
    if (strcmp(sub, "stop") == 0) {
        return handle_queue_stop(argc, argv);
    }
    if (strcmp(sub, "ls") == 0) {
        return handle_queue_ls(argc, argv);
    }

    LOG_ERROR("Unknown sub command '%s' in 'queue'", sub);
    return EXIT_UNKNOWN_COMMAND;
}

static int queue_report_error(const QueueError error) {
    switch (error) {
    case QueueErrorNone:
        return EXIT_SUCCESS;
    case QueueErrorNotFound:
        LOG_ERROR("Queue not found");
        return EXIT_FAILURE;
    case QueueErrorDuplicateName:
        LOG_ERROR("A queue with that name already exists");
        return EXIT_FAILURE;
    case QueueErrorInvalidArg:
        LOG_ERROR("Invalid queue argument");
        return EXIT_FAILURE;
    case QueueErrorPersist:
        LOG_ERROR("Failed to persist queue changes");
        return EXIT_FAILURE;
    default:
        LOG_ERROR("Unknown queue error");
        return EXIT_FAILURE;
    }
}

static bool parse_uint_arg(const char *text, uint32_t *out) {
    if (text == NULL || out == NULL || text[0] == '\0') {
        return false;
    }

    char *end = NULL;
    const unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static void fill_queue_options(const QueueOptions *src, QueueOptions *dst) {
    if (src == NULL || dst == NULL) {
        return;
    }

    *dst = *src;
}

static void fill_queue_patch(const QueuePatch *src, QueuePatch *dst) {
    if (src == NULL || dst == NULL) {
        return;
    }

    *dst = *src;
}

static int build_queue_options(arg_str_t *description, arg_str_t *max_concurrent,
                               arg_str_t *max_connections, arg_str_t *temp_path,
                               arg_str_t *download_path, QueueOptions *out) {
    QueueOptions options = {0};

    if (description->count > 0) {
        options.description = description->sval[0];
    }
    if (max_concurrent->count > 0) {
        if (!parse_uint_arg(max_concurrent->sval[0], &options.max_concurrent_downloads)) {
            LOG_ERROR("Invalid value for --maxConcurrentDownloads");
            return EXIT_FAILURE;
        }
    }
    if (max_connections->count > 0) {
        if (!parse_uint_arg(max_connections->sval[0], &options.max_connections)) {
            LOG_ERROR("Invalid value for --maxConnections");
            return EXIT_FAILURE;
        }
    }
    if (temp_path->count > 0) {
        options.temp_path = temp_path->sval[0];
    }
    if (download_path->count > 0) {
        options.download_path = download_path->sval[0];
    }

    fill_queue_options(&options, out);
    return EXIT_SUCCESS;
}

static int build_queue_patch(arg_str_t *description, arg_str_t *max_concurrent,
                             arg_str_t *max_connections, arg_str_t *temp_path,
                             arg_str_t *download_path, QueuePatch *out) {
    QueuePatch patch = {0};

    if (description->count > 0) {
        patch.set_description = true;
        patch.description = description->sval[0];
    }
    if (max_concurrent->count > 0) {
        patch.set_max_concurrent_downloads = true;
        if (!parse_uint_arg(max_concurrent->sval[0], &patch.max_concurrent_downloads)) {
            LOG_ERROR("Invalid value for --maxConcurrentDownloads");
            return EXIT_FAILURE;
        }
    }
    if (max_connections->count > 0) {
        patch.set_max_connections = true;
        if (!parse_uint_arg(max_connections->sval[0], &patch.max_connections)) {
            LOG_ERROR("Invalid value for --maxConnections");
            return EXIT_FAILURE;
        }
    }
    if (temp_path->count > 0) {
        patch.set_temp_path = true;
        patch.temp_path = temp_path->sval[0];
    }
    if (download_path->count > 0) {
        patch.set_download_path = true;
        patch.download_path = download_path->sval[0];
    }

    fill_queue_patch(&patch, out);
    return EXIT_SUCCESS;
}

static int handle_queue_add(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue add", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *name = arg_str1(NULL, NULL, "NAME", "queue name");
    arg_str_t *description = arg_str0(NULL, "description", "TEXT", "queue description");
    arg_str_t *max_concurrent =
        arg_str0(NULL, "maxConcurrentDownloads", "N", "max concurrent downloads");
    arg_str_t *max_connections = arg_str0(NULL, "maxConnections", "N", "max connections");
    arg_str_t *temp_path = arg_str0(NULL, "tempPath", "PATH", "override temp path");
    arg_str_t *download_path = arg_str0(NULL, "downloadPath", "PATH", "override save path");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {name,        description, max_concurrent, max_connections,
                        temp_path,   download_path, help,           end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (name->count == 0 || name->sval[0] == NULL) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        fatal("Provide a queue name");
    }

    QueueOptions options = {0};
    if (build_queue_options(description, max_concurrent, max_connections, temp_path, download_path,
                           &options) != EXIT_SUCCESS) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    char *id = NULL;
    const QueueError rc = queue_add(name->sval[0], &options, &id);
    cli_free_subargv(sub_argv);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);

    if (rc != QueueErrorNone) {
        return queue_report_error(rc);
    }

    printf("%s\n", id);
    free(id);
    return EXIT_SUCCESS;
}

static int handle_queue_rm(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue rm", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *id = arg_str0(NULL, NULL, "ID", "queue id");
    arg_str_t *by_name = arg_str0(NULL, "name", "NAME", "remove queue by name");
    arg_lit_t *purge_items = arg_lit0(NULL, "purge-items", "remove download items in the queue");
    arg_lit_t *remove_items = arg_lit0(NULL, "remove-items", "alias for --purge-items");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {id, by_name, purge_items, remove_items, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const char *target = NULL;
    bool resolve_by_name = false;
    if (by_name->count > 0 && by_name->sval[0] != NULL) {
        target = by_name->sval[0];
        resolve_by_name = true;
    } else if (id->count > 0 && id->sval[0] != NULL) {
        target = id->sval[0];
    }

    if (target == NULL) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        fatal("Provide a queue id or --name=<name>");
    }

    const bool purge = purge_items->count > 0 || remove_items->count > 0;
    const QueueError rc = queue_remove(target, resolve_by_name, purge);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return queue_report_error(rc);
}

static int handle_queue_edit(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue edit", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *id = arg_str1(NULL, NULL, "ID", "queue id");
    arg_str_t *description = arg_str0(NULL, "description", "TEXT", "queue description");
    arg_str_t *max_concurrent =
        arg_str0(NULL, "maxConcurrentDownloads", "N", "max concurrent downloads");
    arg_str_t *max_connections = arg_str0(NULL, "maxConnections", "N", "max connections");
    arg_str_t *temp_path = arg_str0(NULL, "tempPath", "PATH", "override temp path");
    arg_str_t *download_path = arg_str0(NULL, "downloadPath", "PATH", "override save path");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {id,          description, max_concurrent, max_connections,
                        temp_path,   download_path, help,           end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    if (id->count == 0 || id->sval[0] == NULL) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        fatal("Provide a queue id");
    }

    QueuePatch patch = {0};
    if (build_queue_patch(description, max_concurrent, max_connections, temp_path, download_path,
                          &patch) != EXIT_SUCCESS) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    if (!patch.set_description && !patch.set_max_concurrent_downloads &&
        !patch.set_max_connections && !patch.set_temp_path && !patch.set_download_path) {
        cli_free_subargv(sub_argv);
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        LOG_ERROR("Provide at least one queue option to update");
        return EXIT_FAILURE;
    }

    const QueueError rc = queue_edit(id->sval[0], &patch);
    cli_free_subargv(sub_argv);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return queue_report_error(rc);
}

static int resolve_queue_target(arg_str_t *id, arg_str_t *by_name, char **resolved_out) {
    const char *target = NULL;
    bool resolve_by_name = false;

    if (by_name->count > 0 && by_name->sval[0] != NULL) {
        target = by_name->sval[0];
        resolve_by_name = true;
    } else if (id->count > 0 && id->sval[0] != NULL) {
        target = id->sval[0];
    }

    if (target == NULL) {
        fatal("Provide a queue id or --name=<name>");
    }

    char *resolved = queue_resolve_id(target, resolve_by_name);
    if (resolved == NULL) {
        LOG_ERROR("Queue not found");
        return EXIT_FAILURE;
    }

    *resolved_out = resolved;
    return EXIT_SUCCESS;
}

static int handle_queue_start(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue start", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *id = arg_str0(NULL, NULL, "ID", "queue id");
    arg_str_t *by_name = arg_str0(NULL, "name", "NAME", "start queue by name");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {id, by_name, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    char *resolved = NULL;
    if (resolve_queue_target(id, by_name, &resolved) != EXIT_SUCCESS) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    const QueueError rc = queue_start(resolved);
    free(resolved);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return queue_report_error(rc);
}

static int handle_queue_stop(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue stop", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_str_t *id = arg_str0(NULL, NULL, "ID", "queue id");
    arg_str_t *by_name = arg_str0(NULL, "name", "NAME", "stop queue by name");
    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {id, by_name, help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    char *resolved = NULL;
    if (resolve_queue_target(id, by_name, &resolved) != EXIT_SUCCESS) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return EXIT_FAILURE;
    }

    const QueueError rc = queue_stop(resolved);
    free(resolved);
    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return queue_report_error(rc);
}

static int handle_queue_ls(int argc, char *argv[]) {
    int sub_argc = 0;
    char **sub_argv = NULL;
    if (cli_make_subargv(argc, argv, 3, "avar queue ls", &sub_argc, &sub_argv) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    arg_lit_t *help = arg_lit0("h", "help", "show help");
    arg_end_t *end = arg_end(20);
    void *argtable[] = {help, end};

    bool help_requested = false;
    const int parse_rc = cli_run_argtable(sub_argv[0], argtable, end, sub_argc, sub_argv,
                                          &help_requested);
    cli_free_subargv(sub_argv);

    if (parse_rc != EXIT_SUCCESS || help_requested) {
        arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
        return parse_rc;
    }

    const size_t count = queue_count();
    for (size_t i = 0; i < count; i++) {
        char *id = queue_id_at(i);
        char *name = queue_name_at(i);
        char *description =
            get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_FIELD_DESCRIPTION);
        char *started =
            get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_QUEUE_FIELD_STARTED);

        printf("%s\t%s", id != NULL ? id : "", name != NULL ? name : "");
        if (description != NULL && description[0] != '\0') {
            printf("\t%s", description);
        }
        if (started != NULL && strcmp(started, "true") == 0) {
            printf("\t(running)");
        }
        putchar('\n');

        free(id);
        free(name);
        free(description);
        free(started);
    }

    arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
    return EXIT_SUCCESS;
}
