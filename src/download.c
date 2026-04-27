#include <avar.h>
#include <download.h>
#include <download_cli_handler.h>

int handle_download(int argc, char *argv[]) {
    if (argc < 3) {
        fatal("Expected at least 3 parameters");
    }

    struct download_DocoptArgs args = parse_download_docopt(argc, argv, /* help */ 1, /* version */ VERSION_STR);

    const char *cmd = argv[2];

    LOG_ERROR("Unknown sub command '%s' in 'download'", cmd);
    return EXIT_UNKNOWN_COMMAND;
}

int transient_download() {
    return EXIT_SUCCESS;
}
