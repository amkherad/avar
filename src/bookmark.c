#include <cJSON.h>

#include <avar.h>
#include <bookmark.h>
#include <config.h>
#include <file-system.h>
#include <logger.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AVAR_BOOKMARKS_ROOT_KEY "bookmarks"
#define AVAR_BOOKMARK_ID_PREFIX "bm"
#define AVAR_BOOKMARK_ID_BUF_SIZE 64U

static char *resolve_bookmarks_path(void);
static cJSON *load_bookmarks_root(void);
static int save_bookmarks_root(cJSON *root);
static char *generate_bookmark_id(void);
static cJSON *bookmarks_array(cJSON *root);
static int find_bookmark_index_by_field(cJSON *array, const char *field, const char *value);

static char *resolve_bookmarks_path(void) {
    char *configured = get_config(AVAR_CFG_BOOKMARKS_FILE_PATH);
    if (configured != NULL && configured[0] != '\0') {
        return configured;
    }
    free(configured);

    char *dir = config_get_directory();
    if (dir == NULL) {
        return NULL;
    }

    const size_t path_len =
        strlen(dir) + sizeof(PATH_SEPARATOR) + strlen(AVAR_BOOKMARKS_FILENAME) + 1U;
    char *path = malloc(path_len);
    if (path == NULL) {
        free(dir);
        return NULL;
    }

    snprintf(path, path_len, "%s%c%s", dir, PATH_SEPARATOR, AVAR_BOOKMARKS_FILENAME);
    free(dir);
    return path;
}

static cJSON *load_json_file(const char *path) {
    if (path == NULL) {
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return cJSON_CreateObject();
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return cJSON_CreateObject();
    }

    const long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return cJSON_CreateObject();
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return cJSON_CreateObject();
    }

    char *buffer = malloc((size_t)size + 1U);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    const size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[read] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return cJSON_CreateObject();
    }

    return root;
}

static int persist_json_file(const char *path, cJSON *root) {
    if (path == NULL || root == NULL) {
        return -1;
    }

    char *dir = strdup(path);
    if (dir == NULL) {
        return -1;
    }

    char *last_sep = strrchr(dir, PATH_SEPARATOR);
#if defined(_WIN32)
    {
        char *alt_sep = strrchr(dir, '/');
        if (alt_sep != NULL && (last_sep == NULL || alt_sep > last_sep)) {
            last_sep = alt_sep;
        }
    }
#endif
    if (last_sep != NULL) {
        *last_sep = '\0';
        if (make_dirs_in_path(dir) != 0) {
            free(dir);
            return -1;
        }
    }
    free(dir);

    char *json = cJSON_Print(root);
    if (json == NULL) {
        return -1;
    }

    const size_t path_len = strlen(path);
    char *tmp_path = malloc(path_len + sizeof(AVAR_CONFIG_TMP_SUFFIX));
    if (tmp_path == NULL) {
        cJSON_free(json);
        return -1;
    }

    snprintf(tmp_path, path_len + sizeof(AVAR_CONFIG_TMP_SUFFIX), "%s%s", path,
             AVAR_CONFIG_TMP_SUFFIX);

    FILE *file = fopen(tmp_path, "wb");
    if (file == NULL) {
        free(tmp_path);
        cJSON_free(json);
        return -1;
    }

    const size_t json_len = strlen(json);
    const size_t written = fwrite(json, 1, json_len, file);
    const int sync_rc = file_sync_to_disk(file);
    fclose(file);
    cJSON_free(json);

    if (written != json_len || sync_rc != 0) {
        remove(tmp_path);
        free(tmp_path);
        LOG_ERROR("Failed to write bookmark file: %s", tmp_path);
        return -1;
    }

    if (move_file_atomic(tmp_path, path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        LOG_ERROR("Failed to replace bookmark file: %s (%s)", path, strerror(errno));
        return -1;
    }

    free(tmp_path);
    return 0;
}

static cJSON *load_bookmarks_root(void) {
    char *path = resolve_bookmarks_path();
    if (path == NULL) {
        return NULL;
    }

    cJSON *root = load_json_file(path);
    free(path);
    return root;
}

static int save_bookmarks_root(cJSON *root) {
    if (root == NULL) {
        return -1;
    }

    char *path = resolve_bookmarks_path();
    if (path == NULL) {
        return -1;
    }

    const int rc = persist_json_file(path, root);
    free(path);
    return rc;
}

static cJSON *bookmarks_array(cJSON *root) {
    if (root == NULL) {
        return NULL;
    }

    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, AVAR_BOOKMARKS_ROOT_KEY);
    if (array == NULL) {
        array = cJSON_CreateArray();
        if (array == NULL) {
            return NULL;
        }
        cJSON_AddItemToObject(root, AVAR_BOOKMARKS_ROOT_KEY, array);
    } else if (!cJSON_IsArray(array)) {
        cJSON_ReplaceItemInObjectCaseSensitive(root, AVAR_BOOKMARKS_ROOT_KEY,
                                               cJSON_CreateArray());
        array = cJSON_GetObjectItemCaseSensitive(root, AVAR_BOOKMARKS_ROOT_KEY);
    }

    return array;
}

static char *generate_bookmark_id(void) {
    char generated[AVAR_BOOKMARK_ID_BUF_SIZE];
    snprintf(generated, sizeof generated, "%s%llu", AVAR_BOOKMARK_ID_PREFIX,
             (unsigned long long)time(NULL));
    return strdup(generated);
}

static int find_bookmark_index_by_field(cJSON *array, const char *field, const char *value) {
    if (!cJSON_IsArray(array) || field == NULL || value == NULL) {
        return -1;
    }

    const int count = cJSON_GetArraySize(array);
    for (int i = 0; i < count; ++i) {
        const cJSON *entry = cJSON_GetArrayItem(array, i);
        const cJSON *candidate = cJSON_GetObjectItemCaseSensitive(entry, field);
        if (cJSON_IsString(candidate) && candidate->valuestring != NULL &&
            strcmp(candidate->valuestring, value) == 0) {
            return i;
        }
    }

    return -1;
}

bool bookmark_has_url(const char *url) {
    if (url == NULL || url[0] == '\0') {
        return false;
    }

    cJSON *root = load_bookmarks_root();
    if (root == NULL) {
        return false;
    }

    const cJSON *array = cJSON_GetObjectItemCaseSensitive(root, AVAR_BOOKMARKS_ROOT_KEY);
    const bool found =
        cJSON_IsArray(array) &&
        find_bookmark_index_by_field((cJSON *)array, AVAR_FIELD_URL, url) >= 0;
    cJSON_Delete(root);
    return found;
}

BookmarkError bookmark_add(const char *url, const char *title, uint32_t link_count,
                           char **id_out) {
    if (id_out != NULL) {
        *id_out = NULL;
    }

    if (url == NULL || url[0] == '\0') {
        return BookmarkErrorInvalidArg;
    }

    cJSON *root = load_bookmarks_root();
    if (root == NULL) {
        return BookmarkErrorPersist;
    }

    cJSON *array = bookmarks_array(root);
    if (array == NULL) {
        cJSON_Delete(root);
        return BookmarkErrorPersist;
    }

    const time_t now = time(NULL);
    const int existing = find_bookmark_index_by_field(array, AVAR_FIELD_URL, url);
    if (existing >= 0) {
        cJSON *entry = cJSON_GetArrayItem(array, existing);
        if (entry != NULL) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                entry, AVAR_BOOKMARK_FIELD_TITLE,
                cJSON_CreateString(title != NULL ? title : ""));
            cJSON_ReplaceItemInObjectCaseSensitive(entry, AVAR_BOOKMARK_FIELD_LINK_COUNT,
                                                   cJSON_CreateNumber((double)link_count));
            cJSON_ReplaceItemInObjectCaseSensitive(entry, AVAR_BOOKMARK_FIELD_UPDATED_AT,
                                                   cJSON_CreateNumber((double)now));
        }
    } else {
        char *id = generate_bookmark_id();
        if (id == NULL) {
            cJSON_Delete(root);
            return BookmarkErrorPersist;
        }

        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            free(id);
            cJSON_Delete(root);
            return BookmarkErrorPersist;
        }

        cJSON_AddStringToObject(entry, AVAR_FIELD_ID, id);
        cJSON_AddStringToObject(entry, AVAR_FIELD_URL, url);
        cJSON_AddStringToObject(entry, AVAR_BOOKMARK_FIELD_TITLE, title != NULL ? title : "");
        cJSON_AddNumberToObject(entry, AVAR_BOOKMARK_FIELD_LINK_COUNT, (double)link_count);
        cJSON_AddNumberToObject(entry, AVAR_BOOKMARK_FIELD_CREATED_AT, (double)now);
        cJSON_AddNumberToObject(entry, AVAR_BOOKMARK_FIELD_UPDATED_AT, (double)now);
        cJSON_AddItemToArray(array, entry);

        if (id_out != NULL) {
            *id_out = id;
        } else {
            free(id);
        }
    }

    const BookmarkError rc =
        save_bookmarks_root(root) == 0 ? BookmarkErrorNone : BookmarkErrorPersist;
    cJSON_Delete(root);
    return rc;
}

BookmarkError bookmark_remove(const char *id_or_url, bool by_url) {
    if (id_or_url == NULL || id_or_url[0] == '\0') {
        return BookmarkErrorInvalidArg;
    }

    cJSON *root = load_bookmarks_root();
    if (root == NULL) {
        return BookmarkErrorPersist;
    }

    cJSON *array = bookmarks_array(root);
    if (array == NULL) {
        cJSON_Delete(root);
        return BookmarkErrorPersist;
    }

    const char *field = by_url ? AVAR_FIELD_URL : AVAR_FIELD_ID;
    const int index = find_bookmark_index_by_field(array, field, id_or_url);
    if (index < 0) {
        cJSON_Delete(root);
        return BookmarkErrorNotFound;
    }

    cJSON_DeleteItemFromArray(array, index);
    const BookmarkError rc =
        save_bookmarks_root(root) == 0 ? BookmarkErrorNone : BookmarkErrorPersist;
    cJSON_Delete(root);
    return rc;
}

size_t bookmark_count(void) {
    cJSON *root = load_bookmarks_root();
    if (root == NULL) {
        return 0U;
    }

    const cJSON *array = cJSON_GetObjectItemCaseSensitive(root, AVAR_BOOKMARKS_ROOT_KEY);
    const size_t count = cJSON_IsArray(array) ? (size_t)cJSON_GetArraySize(array) : 0U;
    cJSON_Delete(root);
    return count;
}

char *bookmark_list_json(void) {
    cJSON *root = load_bookmarks_root();
    if (root == NULL) {
        return NULL;
    }

    cJSON *array = bookmarks_array(root);
    if (array == NULL) {
        cJSON_Delete(root);
        return NULL;
    }

    char *json = cJSON_PrintUnformatted(array);
    cJSON_Delete(root);
    return json;
}
