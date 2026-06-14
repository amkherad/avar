#include <cJSON.h>
#include <download_state.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *json_get_string(const cJSON *obj, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL) {
        return NULL;
    }
    return strdup(item->valuestring);
}

static uint64_t json_get_u64(const cJSON *obj, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL || !cJSON_IsNumber(item)) {
        return 0;
    }
    return (uint64_t)item->valuedouble;
}

static void json_add_string_or_null(cJSON *obj, const char *key, const char *value) {
    if (value != NULL) {
        cJSON_AddStringToObject(obj, key, value);
    } else {
        cJSON_AddNullToObject(obj, key);
    }
}

void download_state_free(DownloadState *state) {
    if (state == NULL) {
        return;
    }

    free(state->id);
    free(state->url);
    free(state->filename);
    free(state->temp_path);
    free(state->dest_path);
    free(state->status);
    free(state->proxy);
    free(state->queued_at);
    free(state->last_try_at);
    free(state->description);
    free(state->original_page);
    free(state->referer);
    free(state->added_through);
    free(state->queue_id);
    free(state->etag);
    free(state->last_modified);
    free(state->chunks_done);
    free(state);
}

DownloadState *download_state_create(const char *url, const char *filename,
                                     const char *temp_path, const char *dest_path,
                                     const uint64_t total_size, const size_t chunk_size) {
    DownloadState *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return NULL;
    }

    state->url = url != NULL ? strdup(url) : NULL;
    state->filename = filename != NULL ? strdup(filename) : NULL;
    state->temp_path = temp_path != NULL ? strdup(temp_path) : NULL;
    state->dest_path = dest_path != NULL ? strdup(dest_path) : NULL;
    state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
    state->total_size = total_size;
    state->chunk_size = chunk_size > 0 ? chunk_size : DL_CHUNK_SIZE;

    if (total_size > 0) {
        state->chunk_count = (size_t)((total_size + state->chunk_size - 1) / state->chunk_size);
    } else {
        state->chunk_count = 0;
    }

    if (state->chunk_count > 0) {
        state->chunks_done = calloc(state->chunk_count, sizeof(bool));
        if (state->chunks_done == NULL) {
            download_state_free(state);
            return NULL;
        }
    }

    return state;
}

int download_state_init_chunks(DownloadState *state, const uint64_t total_size,
                               const size_t chunk_size) {
    if (state == NULL || total_size == 0) {
        return -1;
    }

    const size_t new_chunk_size = chunk_size > 0 ? chunk_size : DL_CHUNK_SIZE;
    const size_t new_chunk_count =
            (size_t)((total_size + new_chunk_size - 1U) / new_chunk_size);

    bool *new_done = calloc(new_chunk_count, sizeof(bool));
    if (new_done == NULL) {
        return -1;
    }

    if (state->chunks_done != NULL && state->chunk_count > 0) {
        const size_t preserve = state->chunk_count < new_chunk_count ? state->chunk_count
                                                                     : new_chunk_count;
        for (size_t i = 0; i < preserve; i++) {
            new_done[i] = state->chunks_done[i];
        }
    }

    free(state->chunks_done);
    state->chunks_done = new_done;
    state->total_size = total_size;
    state->chunk_size = new_chunk_size;
    state->chunk_count = new_chunk_count;
    return 0;
}

DownloadState *download_state_load(const char *path) {
    if (path == NULL) {
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
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
        return NULL;
    }

    DownloadState *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        cJSON_Delete(root);
        return NULL;
    }

    state->id = json_get_string(root, AVAR_FIELD_ID);
    state->url = json_get_string(root, AVAR_FIELD_URL);
    state->filename = json_get_string(root, AVAR_FIELD_FILENAME);
    state->temp_path = json_get_string(root, AVAR_STATE_FIELD_TEMP_PATH);
    state->dest_path = json_get_string(root, AVAR_STATE_FIELD_DEST_PATH);
    state->status = json_get_string(root, AVAR_FIELD_STATUS);
    state->proxy = json_get_string(root, AVAR_FIELD_PROXY);
    state->queued_at = json_get_string(root, AVAR_FIELD_QUEUED_AT);
    state->last_try_at = json_get_string(root, AVAR_FIELD_LAST_TRY_AT);
    state->description = json_get_string(root, AVAR_FIELD_DESCRIPTION);
    state->original_page = json_get_string(root, AVAR_FIELD_ORIGINAL_PAGE);
    state->referer = json_get_string(root, AVAR_FIELD_REFERER);
    state->added_through = json_get_string(root, AVAR_FIELD_ADDED_THROUGH);
    state->queue_id = json_get_string(root, AVAR_FIELD_QUEUE_ID);
    state->etag = json_get_string(root, AVAR_FIELD_ETAG);
    state->last_modified = json_get_string(root, AVAR_FIELD_LAST_MODIFIED);

    state->total_size = json_get_u64(root, AVAR_FIELD_TOTAL_BYTES);
    if (state->total_size == 0) {
        state->total_size = json_get_u64(root, AVAR_STATE_FIELD_TOTAL_SIZE);
    }
    state->bytes_downloaded = json_get_u64(root, AVAR_FIELD_BYTES_DOWNLOADED);

    const cJSON *chunk_size = cJSON_GetObjectItemCaseSensitive(root, AVAR_STATE_FIELD_CHUNK_SIZE);
    if (chunk_size != NULL && cJSON_IsNumber(chunk_size)) {
        state->chunk_size = (size_t)chunk_size->valuedouble;
    } else {
        state->chunk_size = DL_CHUNK_SIZE;
    }

    const cJSON *chunk_count = cJSON_GetObjectItemCaseSensitive(root, AVAR_STATE_FIELD_CHUNK_COUNT);
    if (chunk_count != NULL && cJSON_IsNumber(chunk_count)) {
        state->chunk_count = (size_t)chunk_count->valuedouble;
    } else if (state->total_size > 0) {
        state->chunk_count =
            (size_t)((state->total_size + state->chunk_size - 1) / state->chunk_size);
    }

    if (state->chunk_count > 0) {
        state->chunks_done = calloc(state->chunk_count, sizeof(bool));
        if (state->chunks_done == NULL) {
            cJSON_Delete(root);
            download_state_free(state);
            return NULL;
        }

        const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(root, AVAR_FIELD_CHUNKS);
        if (chunks != NULL && cJSON_IsArray(chunks)) {
            const int count = cJSON_GetArraySize(chunks);
            for (int i = 0; i < count && (size_t)i < state->chunk_count; i++) {
                const cJSON *item = cJSON_GetArrayItem(chunks, i);
                state->chunks_done[i] = cJSON_IsTrue(item);
            }
        }
    }

    cJSON_Delete(root);

    if (state->url == NULL || state->filename == NULL || state->temp_path == NULL
        || state->dest_path == NULL) {
        download_state_free(state);
        return NULL;
    }

    if (state->added_through == NULL) {
        state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
    }

    return state;
}

int download_state_save(const DownloadState *state, const char *path) {
    if (state == NULL || path == NULL) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }

    json_add_string_or_null(root, AVAR_FIELD_ID, state->id);
    json_add_string_or_null(root, AVAR_FIELD_URL, state->url);
    json_add_string_or_null(root, AVAR_FIELD_FILENAME, state->filename);
    json_add_string_or_null(root, AVAR_FIELD_STATUS, state->status);
    json_add_string_or_null(root, AVAR_FIELD_PROXY, state->proxy);
    cJSON_AddNumberToObject(root, AVAR_FIELD_BYTES_DOWNLOADED, (double) state->bytes_downloaded);
    cJSON_AddNumberToObject(root, AVAR_FIELD_TOTAL_BYTES, (double) state->total_size);
    json_add_string_or_null(root, AVAR_FIELD_QUEUED_AT, state->queued_at);
    json_add_string_or_null(root, AVAR_FIELD_LAST_TRY_AT, state->last_try_at);
    json_add_string_or_null(root, AVAR_FIELD_DESCRIPTION, state->description);
    json_add_string_or_null(root, AVAR_FIELD_ORIGINAL_PAGE, state->original_page);
    json_add_string_or_null(root, AVAR_FIELD_REFERER, state->referer);
    json_add_string_or_null(root, AVAR_FIELD_ADDED_THROUGH,
                            state->added_through != NULL ? state->added_through
                                                         : AVAR_DL_ADDED_DIRECT);
    json_add_string_or_null(root, AVAR_FIELD_QUEUE_ID, state->queue_id);

    if (state->temp_path != NULL) {
        cJSON_AddStringToObject(root, AVAR_STATE_FIELD_TEMP_PATH, state->temp_path);
    }
    if (state->dest_path != NULL) {
        cJSON_AddStringToObject(root, AVAR_STATE_FIELD_DEST_PATH, state->dest_path);
    }
    if (state->etag != NULL) {
        cJSON_AddStringToObject(root, AVAR_FIELD_ETAG, state->etag);
    }
    if (state->last_modified != NULL) {
        cJSON_AddStringToObject(root, AVAR_FIELD_LAST_MODIFIED, state->last_modified);
    }

    cJSON_AddNumberToObject(root, AVAR_STATE_FIELD_CHUNK_SIZE, (double) state->chunk_size);
    cJSON_AddNumberToObject(root, AVAR_STATE_FIELD_CHUNK_COUNT, (double) state->chunk_count);

    cJSON *chunks = cJSON_AddArrayToObject(root, AVAR_FIELD_CHUNKS);
    if (chunks == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    for (size_t i = 0; i < state->chunk_count; i++) {
        cJSON_AddItemToArray(chunks, cJSON_CreateBool(state->chunks_done[i]));
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return -1;
    }

    const size_t path_len = strlen(path);
    char *tmp_path = malloc(path_len + sizeof(AVAR_STATE_TMP_SUFFIX));
    if (tmp_path == NULL) {
        cJSON_free(json);
        return -1;
    }
    snprintf(tmp_path, path_len + sizeof(AVAR_STATE_TMP_SUFFIX), "%s%s", path,
             AVAR_STATE_TMP_SUFFIX);

    FILE *file = fopen(tmp_path, "wb");
    if (file == NULL) {
        free(tmp_path);
        cJSON_free(json);
        return -1;
    }

    const size_t json_len = strlen(json);
    const size_t written = fwrite(json, 1, json_len, file);
    fflush(file);
    fclose(file);
    cJSON_free(json);

    if (written != json_len) {
        remove(tmp_path);
        free(tmp_path);
        return -1;
    }

#if defined(_WIN32)
    remove(path);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        return -1;
    }

    free(tmp_path);
    return 0;
}

size_t download_state_completed_chunks(const DownloadState *state) {
    if (state == NULL || state->chunks_done == NULL) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < state->chunk_count; i++) {
        if (state->chunks_done[i]) {
            count++;
        }
    }
    return count;
}

uint64_t download_state_bytes_done(const DownloadState *state) {
    if (state == NULL || state->chunks_done == NULL || state->chunk_count == 0) {
        return state != NULL ? state->bytes_downloaded : 0;
    }

    uint64_t bytes = 0;
    for (size_t i = 0; i < state->chunk_count; i++) {
        if (!state->chunks_done[i]) {
            continue;
        }

        const uint64_t start = (uint64_t)i * (uint64_t)state->chunk_size;
        uint64_t end = start + (uint64_t)state->chunk_size - 1;
        if (state->total_size > 0 && end >= state->total_size) {
            end = state->total_size - 1;
        }
        bytes += end - start + 1;
    }
    return bytes;
}

bool download_state_all_chunks_done(const DownloadState *state) {
    if (state == NULL || state->chunk_count == 0) {
        return false;
    }

    return download_state_completed_chunks(state) == state->chunk_count;
}
