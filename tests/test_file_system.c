#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"

static TestGuard g_guard;

static void setup_paths(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-fs"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(file_system_sanitize_filename) {
    AVAR_ASSERT_NULL(sanitize_filename(NULL));
    AVAR_ASSERT_NULL(sanitize_filename(""));
    AVAR_ASSERT_NULL(sanitize_filename("   "));

    char *clean = sanitize_filename("hello/world?.txt");
    AVAR_ASSERT_NOT_NULL(clean);
    AVAR_ASSERT_STR_EQ(clean, "helloworld.txt");
    free(clean);

    char *trimmed = sanitize_filename(" report.pdf. ");
    AVAR_ASSERT_NOT_NULL(trimmed);
    AVAR_ASSERT(strstr(trimmed, "report.pdf") != NULL);
    free(trimmed);
}

AVAR_TEST(file_system_path_join) {
    AVAR_ASSERT_NULL(path_join(NULL, "a"));
    AVAR_ASSERT_NULL(path_join("dir", NULL));

    char *joined = path_join("/tmp", "file.txt");
    AVAR_ASSERT_NOT_NULL(joined);
#if defined(_WIN32)
    AVAR_ASSERT(strstr(joined, "file.txt") != NULL);
#else
    AVAR_ASSERT_STR_EQ(joined, "/tmp/file.txt");
#endif
    free(joined);
}

AVAR_TEST(file_system_make_dirs_and_exists) {
    setup_paths();

    char subdir[512];
    snprintf(subdir, sizeof subdir, "%s%cnested%cdir", g_guard.work_dir, PATH_SEPARATOR,
             PATH_SEPARATOR);
    AVAR_ASSERT_EQ(make_dirs_in_path(subdir), 0);

    char *file_path = path_join(subdir, "artifact.bin");
    AVAR_ASSERT_NOT_NULL(file_path);

    FILE *fp = fopen(file_path, "wb");
    AVAR_ASSERT_NOT_NULL(fp);
    AVAR_ASSERT_EQ(fwrite("data", 1, 4, fp), 4U);
    fclose(fp);

    AVAR_ASSERT(file_exists(file_path));
    AVAR_ASSERT(!file_exists(subdir));

    char *missing = path_join(subdir, "missing.bin");
    AVAR_ASSERT_NOT_NULL(missing);
    AVAR_ASSERT(!file_exists(missing));
    free(missing);
    free(file_path);
}

AVAR_TEST(file_system_move_file_atomic) {
    setup_paths();

    char *src = path_join(g_guard.work_dir, "source.bin");
    char *dest = path_join(g_guard.work_dir, "dest.bin");
    AVAR_ASSERT_NOT_NULL(src);
    AVAR_ASSERT_NOT_NULL(dest);

    FILE *fp = fopen(src, "wb");
    AVAR_ASSERT_NOT_NULL(fp);
    AVAR_ASSERT_EQ(fwrite("payload", 1, 7, fp), 7U);
    fclose(fp);

    AVAR_ASSERT_EQ(move_file_atomic(src, dest), 0);
    AVAR_ASSERT(!file_exists(src));
    AVAR_ASSERT(file_exists(dest));

    free(src);
    free(dest);
}

AVAR_TEST(file_system_remove_directory_recursive) {
    setup_paths();

    char *tree = path_join(g_guard.work_dir, "tree");
    AVAR_ASSERT_NOT_NULL(tree);
    AVAR_ASSERT_EQ(make_dirs_in_path(tree), 0);

    char *nested = path_join(tree, "child");
    AVAR_ASSERT_NOT_NULL(nested);
    AVAR_ASSERT_EQ(make_dirs_in_path(nested), 0);

    char *leaf = path_join(nested, "leaf.txt");
    AVAR_ASSERT_NOT_NULL(leaf);
    FILE *fp = fopen(leaf, "wb");
    AVAR_ASSERT_NOT_NULL(fp);
    fclose(fp);

    AVAR_ASSERT_EQ(remove_directory_recursive(tree), 0);
    AVAR_ASSERT(!file_exists(leaf));

    free(leaf);
    free(nested);
    free(tree);
}

AVAR_TEST(file_system_config_path_uses_home) {
#if defined(_WIN32)
    const char *key = AVAR_ENV_USERPROFILE;
#else
    const char *key = AVAR_ENV_HOME;
#endif
    const char *saved = getenv(key);
    char backup[512] = {0};
    if (saved != NULL) {
        snprintf(backup, sizeof backup, "%s", saved);
    }

#if defined(_WIN32)
    (void)_putenv(AVAR_ENV_USERPROFILE "=C:\\avar-test-home");
#else
    const char *xdg_saved = getenv(AVAR_ENV_XDG_CONFIG_HOME);
    char xdg_backup[512] = {0};
    if (xdg_saved != NULL) {
        snprintf(xdg_backup, sizeof xdg_backup, "%s", xdg_saved);
    }
    AVAR_ASSERT_EQ(unsetenv(AVAR_ENV_XDG_CONFIG_HOME), 0);
    AVAR_ASSERT_EQ(setenv(AVAR_ENV_HOME, "/avar-test-home", 1), 0);
#endif

    char *path = config_path("avar-test");
    AVAR_ASSERT_NOT_NULL(path);
#if defined(_WIN32)
    AVAR_ASSERT(strstr(path, "avar-test") != NULL);
#else
    AVAR_ASSERT(strstr(path, "/avar-test-home/avar-test") != NULL);
#endif
    free(path);

    if (backup[0] != '\0') {
#if defined(_WIN32)
        char entry[640];
        snprintf(entry, sizeof entry, "%s=%s", key, backup);
        (void)_putenv(entry);
#else
        AVAR_ASSERT_EQ(setenv(key, backup, 1), 0);
#endif
    } else {
#if !defined(_WIN32)
        AVAR_ASSERT_EQ(unsetenv(key), 0);
#endif
    }

#if !defined(_WIN32)
    if (xdg_backup[0] != '\0') {
        AVAR_ASSERT_EQ(setenv(AVAR_ENV_XDG_CONFIG_HOME, xdg_backup, 1), 0);
    } else {
        AVAR_ASSERT_EQ(unsetenv(AVAR_ENV_XDG_CONFIG_HOME), 0);
    }
#endif
}

AVAR_TEST(file_system_default_paths_from_config) {
    setup_paths();

    char temp_path[512];
    char download_path[512];
    snprintf(temp_path, sizeof temp_path, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    snprintf(download_path, sizeof download_path, "%s%cdownloads", g_guard.work_dir,
             PATH_SEPARATOR);

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, temp_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, download_path), 0);

    char *temp = default_temp_path();
    char *download = default_download_path();
    AVAR_ASSERT_NOT_NULL(temp);
    AVAR_ASSERT_NOT_NULL(download);
    AVAR_ASSERT_STR_EQ(temp, temp_path);
    AVAR_ASSERT_STR_EQ(download, download_path);
    free(temp);
    free(download);
}

AVAR_TEST_MAIN(
        run_file_system_sanitize_filename();
        run_file_system_path_join();
        run_file_system_make_dirs_and_exists();
        run_file_system_move_file_atomic();
        run_file_system_remove_directory_recursive();
        run_file_system_config_path_uses_home();
        run_file_system_default_paths_from_config();)
