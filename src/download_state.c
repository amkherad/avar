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
    state->added_through = strdup("direct");
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

    state->id = json_get_string(root, "id");
    state->url = json_get_string(root, "url");
    state->filename = json_get_string(root, "filename");
    state->temp_path = json_get_string(root, "temp_path");
    state->dest_path = json_get_string(root, "dest_path");
    state->status = json_get_string(root, "status");
    state->proxy = json_get_string(root, "proxy");
    state->queued_at = json_get_string(root, "queuedAt");
    state->last_try_at = json_get_string(root, "lastTryAt");
    state->description = json_get_string(root, "description");
    state->original_page = json_get_string(root, "originalPage");
    state->referer = json_get_string(root, "referer");
    state->added_through = json_get_string(root, "addedThrough");
    state->queue_id = json_get_string(root, "queueId");
    state->etag = json_get_string(root, "etag");
    state->last_modified = json_get_string(root, "last_modified");

    state->total_size = json_get_u64(root, "totalBytes");
    if (state->total_size == 0) {
        state->total_size = json_get_u64(root, "total_size");
    }
    state->bytes_downloaded = json_get_u64(root, "bytesDownloaded");

    const cJSON *chunk_size = cJSON_GetObjectItemCaseSensitive(root, "chunk_size");
    if (chunk_size != NULL && cJSON_IsNumber(chunk_size)) {
        state->chunk_size = (size_t)chunk_size->valuedouble;
    } else {
        state->chunk_size = DL_CHUNK_SIZE;
    }

    const cJSON *chunk_count = cJSON_GetObjectItemCaseSensitive(root, "chunk_count");
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

        const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(root, "chunks");
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
        state->added_through = strdup("direct");
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

    json_add_string_or_null(root, "id", state->id);
    json_add_string_or_null(root, "url", state->url);
    json_add_string_or_null(root, "filename", state->filename);
    json_add_string_or_null(root, "status", state->status);
    json_add_string_or_null(root, "proxy", state->proxy);
    cJSON_AddNumberToObject(root, "bytesDownloaded", (double)state->bytes_downloaded);
    cJSON_AddNumberToObject(root, "totalBytes", (double)state->total_size);
    json_add_string_or_null(root, "queuedAt", state->queued_at);
    json_add_string_or_null(root, "lastTryAt", state->last_try_at);
    json_add_string_or_null(root, "description", state->description);
    json_add_string_or_null(root, "originalPage", state->original_page);
    json_add_string_or_null(root, "referer", state->referer);
    json_add_string_or_null(root, "addedThrough",
                            state->added_through != NULL ? state->added_through : "direct");
    json_add_string_or_null(root, "queueId", state->queue_id);

    if (state->temp_path != NULL) {
        cJSON_AddStringToObject(root, "temp_path", state->temp_path);
    }
    if (state->dest_path != NULL) {
        cJSON_AddStringToObject(root, "dest_path", state->dest_path);
    }
    if (state->etag != NULL) {
        cJSON_AddStringToObject(root, "etag", state->etag);
    }
    if (state->last_modified != NULL) {
        cJSON_AddStringToObject(root, "last_modified", state->last_modified);
    }

    cJSON_AddNumberToObject(root, "chunk_size", (double)state->chunk_size);
    cJSON_AddNumberToObject(root, "chunk_count", (double)state->chunk_count);

    cJSON *chunks = cJSON_AddArrayToObject(root, "chunks");
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
    char *tmp_path = malloc(path_len + 5);
    if (tmp_path == NULL) {
        cJSON_free(json);
        return -1;
    }
    snprintf(tmp_path, path_len + 5, "%s.tmp", path);

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
