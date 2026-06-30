#include <download_io.h>

#include <avar.h>
#include <utils.h>

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static _Thread_local bool g_download_io_scope = false;
static _Thread_local char g_download_io_item_id[AVAR_DL_ID_BUF_SIZE];

static atomic_uint g_download_io_scope_count = 0;

void download_io_scope_begin(const char *item_id) {
    g_download_io_scope = true;
    g_download_io_item_id[0] = '\0';
    if (item_id != NULL) {
        snprintf(g_download_io_item_id, sizeof g_download_io_item_id, "%s", item_id);
    }
    atomic_fetch_add_explicit(&g_download_io_scope_count, 1U, memory_order_acq_rel);
}

void download_io_scope_end(void) {
    g_download_io_scope = false;
    g_download_io_item_id[0] = '\0';
    atomic_fetch_sub_explicit(&g_download_io_scope_count, 1U, memory_order_acq_rel);
}

static char *item_id_from_state_path(const char *path) {
    if (path == NULL) {
        return NULL;
    }

    const char *last_sep = strrchr(path, PATH_SEPARATOR);
    if (last_sep == NULL) {
        return NULL;
    }

    if (strcmp(last_sep + 1, DL_STATE_FILENAME) != 0) {
        return NULL;
    }

    if (last_sep == path) {
        return NULL;
    }

    const char *dir_end = last_sep;
    const char *cursor = dir_end - 1;
    while (cursor > path && *cursor != PATH_SEPARATOR) {
        cursor--;
    }

    const char *dir_start = (*cursor == PATH_SEPARATOR) ? cursor + 1 : path;
    if (dir_start >= dir_end) {
        return NULL;
    }

    return strndup(dir_start, (size_t)(dir_end - dir_start));
}

bool download_io_state_write_allowed(const char *path) {
    if (atomic_load_explicit(&g_download_io_scope_count, memory_order_acquire) == 0U) {
        return true;
    }

    if (g_download_io_scope) {
        char *item_id = item_id_from_state_path(path);
        if (item_id == NULL) {
            return true;
        }

        const bool allowed =
            g_download_io_item_id[0] != '\0' && strcmp(g_download_io_item_id, item_id) == 0;
        free(item_id);
        return allowed;
    }

    return false;
}
