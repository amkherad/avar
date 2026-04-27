#include <avar.h>
#include <queue.h>
#include <queue_cli_handler.h>

int handle_queue(int argc, char *argv[]) {
    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    struct queue_DocoptArgs args = parse_queue_docopt(argc, argv, /* help */ 1, /* version */ VERSION_STR);

    const char *cmd = argv[2];

    if (args.add) {
        printf("%s", args.name);
        if (!args.name) {
            fatal("Provide a queue name");
        }
    }

    LOG_ERROR("Unknown sub command '%s' in 'queue'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}
