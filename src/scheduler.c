#include <avar.h>
#include <scheduler.h>
#include <scheduler_cli_handler.h>

int handle_scheduler(int argc, char *argv[]) {
    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    struct scheduler_DocoptArgs args = parse_scheduler_docopt(argc, argv, /* help */ 1, /* version */ VERSION_STR);

    const char *cmd = argv[2];

    LOG_ERROR("Unknown sub command '%s' in 'scheduler'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}
