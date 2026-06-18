#include <cli.h>
#include <mongoose_log.h>

int main(int argc, char *argv[]) {
    avar_mongoose_log_init();
    return cli_run(argc, argv);
}
