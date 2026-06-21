#include <cli.h>
#include <mongoose_log.h>

#if defined(AVAR_EMBED_GUI)
#include <cli/gui_launch.h>
#endif

int main(int argc, char *argv[]) {
    avar_mongoose_log_init();

#if defined(AVAR_EMBED_GUI)
    const int embed_rc = cli_embed_gui_early(argc, argv);
    if (embed_rc >= 0) {
        return embed_rc;
    }
#endif

    return cli_run(argc, argv);
}
