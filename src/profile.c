#include <avar.h>
#include <profile.h>
#include <profile_cli_handler.h>

int handle_profile(int argc, char *argv[]) {
    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    struct profile_DocoptArgs args = parse_profile_docopt(argc, argv, /* help */ 1, /* version */ VERSION_STR);

    const char *cmd = argv[2];

    LOG_ERROR("Unknown sub command '%s' in 'profile'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}
