#include "avar_test.h"

#include <cli.h>

#include <stdlib.h>
#include <string.h>

AVAR_TEST(cli_is_help_flag_variants) {
    AVAR_ASSERT(cli_is_help_flag("-h"));
    AVAR_ASSERT(cli_is_help_flag("--help"));
    AVAR_ASSERT(!cli_is_help_flag("--verbose"));
    AVAR_ASSERT(!cli_is_help_flag(NULL));
}

AVAR_TEST(cli_make_subargv_splits_tail) {
    char *argv[] = {"avar", "queue", "ls", "--help"};
    int sub_argc = 0;
    char **sub_argv = NULL;

    AVAR_ASSERT_EQ(cli_make_subargv(4, argv, 2, "queue", &sub_argc, &sub_argv), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(sub_argc, 3);
    AVAR_ASSERT_STR_EQ(sub_argv[0], "queue");
    AVAR_ASSERT_STR_EQ(sub_argv[1], "ls");
    AVAR_ASSERT_STR_EQ(sub_argv[2], "--help");
    cli_free_subargv(sub_argv);
}

AVAR_TEST(cli_parse_avar_help) {
    char *argv[] = {"avar", "-h"};
    AvarArgs args = {0};
    AVAR_ASSERT_EQ(cli_parse_avar(2, argv, &args), EXIT_SUCCESS);
    AVAR_ASSERT(args.help);
}

AVAR_TEST(cli_parse_avar_version) {
    char *argv[] = {"avar", "--version"};
    AvarArgs args = {0};
    AVAR_ASSERT_EQ(cli_parse_avar(2, argv, &args), EXIT_SUCCESS);
    AVAR_ASSERT(args.version);
}

AVAR_TEST(cli_parse_avar_url) {
    char *argv[] = {"avar", "https://example.com/file.bin", "--queue=default"};
    AvarArgs args = {0};
    AVAR_ASSERT_EQ(cli_parse_avar(3, argv, &args), EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(args.url);
    AVAR_ASSERT_STR_EQ(args.url, "https://example.com/file.bin");
    AVAR_ASSERT_NOT_NULL(args.queue);
    AVAR_ASSERT_STR_EQ(args.queue, "default");
}

AVAR_TEST(cli_run_without_args_prints_help) {
    char *argv[] = {"avar"};
    const int rc = cli_run(1, argv);
    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);
}

AVAR_TEST(cli_run_unknown_command) {
    char *argv[] = {"avar", "not-a-command"};
    AVAR_ASSERT_EQ(cli_run(2, argv), EXIT_UNKNOWN_COMMAND);
}

AVAR_TEST(cli_run_verbose_flag) {
    char *argv[] = {"avar", "--verbose", "config", "--help"};
    AVAR_ASSERT_EQ(cli_run(4, argv), EXIT_SUCCESS);
}

AVAR_TEST(cli_run_dl_alias) {
    char *argv[] = {"avar", "dl", "--help"};
    AVAR_ASSERT_EQ(cli_run(3, argv), EXIT_SUCCESS);
}

AVAR_TEST_MAIN(
        run_cli_is_help_flag_variants();
        run_cli_make_subargv_splits_tail();
        run_cli_parse_avar_help();
        run_cli_parse_avar_version();
        run_cli_parse_avar_url();
        run_cli_run_without_args_prints_help();
        run_cli_run_unknown_command();
        run_cli_run_verbose_flag();
        run_cli_run_dl_alias();)
