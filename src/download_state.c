#include <cJSON.h>
#include <download_state.h>
#include <download_io.h>

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

static int ensure_range_capacity(DownloadState *state, const size_t needed) {
    if (state == NULL || needed <= state->done_range_capacity) {
        return 0;
    }

    size_t new_cap = state->done_range_capacity == 0 ? 4U : state->done_range_capacity * 2U;
    while (new_cap < needed) {
        new_cap *= 2U;
    }

    ByteRange *ranges = realloc(state->done_ranges, new_cap * sizeof(*ranges));
    if (ranges == NULL) {
        return -1;
    }

    state->done_ranges = ranges;
    state->done_range_capacity = new_cap;
    return 0;
}

static void coalesce_done_ranges(DownloadState *state) {
    if (state == NULL || state->done_range_count <= 1U) {
        return;
    }

    size_t write = 0U;
    for (size_t read = 1U; read < state->done_range_count; read++) {
        ByteRange *current = &state->done_ranges[write];
        const ByteRange *next = &state->done_ranges[read];

        if (next->start <= current->end + 1U) {
            if (next->end > current->end) {
                current->end = next->end;
            }
            continue;
        }

        write++;
        state->done_ranges[write] = *next;
    }

    state->done_range_count = write + 1U;
}

static void clip_done_ranges(DownloadState *state) {
    if (state == NULL || state->done_range_count == 0U) {
        return;
    }

    size_t write = 0U;
    const uint64_t max_end =
            state->total_size > 0U ? state->total_size - 1U : UINT64_MAX;

    for (size_t read = 0U; read < state->done_range_count; read++) {
        ByteRange range = state->done_ranges[read];
        if (state->total_size > 0U) {
            if (range.start >= state->total_size) {
                continue;
            }
            if (range.end > max_end) {
                range.end = max_end;
            }
        }

        if (range.start > range.end) {
            continue;
        }

        state->done_ranges[write++] = range;
    }

    state->done_range_count = write;
    coalesce_done_ranges(state);
}

static int insert_sorted_range(DownloadState *state, const ByteRange range) {
    if (ensure_range_capacity(state, state->done_range_count + 1U) != 0) {
        return -1;
    }

    size_t insert_at = 0U;
    while (insert_at < state->done_range_count
           && state->done_ranges[insert_at].start < range.start) {
        insert_at++;
    }

    if (insert_at < state->done_range_count) {
        memmove(&state->done_ranges[insert_at + 1U], &state->done_ranges[insert_at],
                (state->done_range_count - insert_at) * sizeof(ByteRange));
    }

    state->done_ranges[insert_at] = range;
    state->done_range_count++;
    return 0;
}

int download_state_mark_range_done(DownloadState *state, uint64_t start, uint64_t end) {
    if (state == NULL || start > end) {
        return -1;
    }

    if (state->total_size > 0U && start >= state->total_size) {
        return 0;
    }

    if (state->total_size > 0U && end >= state->total_size) {
        end = state->total_size - 1U;
    }

    if (download_state_is_range_done(state, start, end)) {
        return 0;
    }

    size_t lo = 0U;
    while (lo < state->done_range_count && state->done_ranges[lo].end + 1U < start) {
        lo++;
    }

    uint64_t merge_start = start;
    uint64_t merge_end = end;
    size_t hi = lo;
    while (hi < state->done_range_count && state->done_ranges[hi].start <= merge_end + 1U) {
        if (state->done_ranges[hi].start < merge_start) {
            merge_start = state->done_ranges[hi].start;
        }
        if (state->done_ranges[hi].end > merge_end) {
            merge_end = state->done_ranges[hi].end;
        }
        hi++;
    }

    const ByteRange merged = {.start = merge_start, .end = merge_end};
    if (hi > lo) {
        state->done_ranges[lo] = merged;
        if (hi < state->done_range_count) {
            memmove(&state->done_ranges[lo + 1U], &state->done_ranges[hi],
                    (state->done_range_count - hi) * sizeof(ByteRange));
        }
        state->done_range_count -= hi - lo - 1U;
        return 0;
    }

    return insert_sorted_range(state, merged);
}

bool download_state_is_range_done(const DownloadState *state, const uint64_t start,
                                  const uint64_t end) {
    if (state == NULL || start > end) {
        return false;
    }

    for (size_t i = 0U; i < state->done_range_count; i++) {
        const ByteRange *range = &state->done_ranges[i];
        if (range->end < start) {
            continue;
        }
        if (range->start > start) {
            return false;
        }
        if (range->end >= end) {
            return true;
        }
    }

    return false;
}

size_t download_state_segment_count(const DownloadState *state) {
    if (state == NULL || state->total_size == 0U) {
        return 0U;
    }

    const size_t chunk_size = state->chunk_size > 0U ? state->chunk_size : DL_CHUNK_SIZE;
    return (size_t)((state->total_size + chunk_size - 1U) / chunk_size);
}

bool download_state_is_segment_done(const DownloadState *state, const size_t segment_index) {
    if (state == NULL || state->total_size == 0U) {
        return false;
    }

    const size_t chunk_size = state->chunk_size > 0U ? state->chunk_size : DL_CHUNK_SIZE;
    const uint64_t start = (uint64_t)segment_index * (uint64_t)chunk_size;
    uint64_t end = start + (uint64_t)chunk_size - 1U;
    if (end >= state->total_size) {
        end = state->total_size - 1U;
    }

    return download_state_is_range_done(state, start, end);
}

static int load_legacy_chunks(DownloadState *state, const cJSON *chunks, const size_t chunk_count) {
    if (state == NULL || chunks == NULL || !cJSON_IsArray(chunks) || chunk_count == 0U) {
        return 0;
    }

    const size_t chunk_size = state->chunk_size > 0U ? state->chunk_size : DL_CHUNK_SIZE;
    const int count = cJSON_GetArraySize(chunks);
    const size_t limit = (size_t)count < chunk_count ? (size_t)count : chunk_count;

    for (size_t i = 0U; i < limit; i++) {
        const cJSON *item = cJSON_GetArrayItem(chunks, (int)i);
        if (!cJSON_IsTrue(item)) {
            continue;
        }

        const uint64_t start = (uint64_t)i * (uint64_t)chunk_size;
        uint64_t end = start + (uint64_t)chunk_size - 1U;
        if (state->total_size > 0U && end >= state->total_size) {
            end = state->total_size - 1U;
        }

        if (download_state_mark_range_done(state, start, end) != 0) {
            return -1;
        }
    }

    return 0;
}

static int load_done_ranges(DownloadState *state, const cJSON *ranges) {
    if (state == NULL || ranges == NULL || !cJSON_IsArray(ranges)) {
        return -1;
    }

    const int count = cJSON_GetArraySize(ranges);
    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(ranges, i);
        if (item == NULL || !cJSON_IsArray(item) || cJSON_GetArraySize(item) < 2) {
            continue;
        }

        const cJSON *start_item = cJSON_GetArrayItem(item, 0);
        const cJSON *end_item = cJSON_GetArrayItem(item, 1);
        if (!cJSON_IsNumber(start_item) || !cJSON_IsNumber(end_item)) {
            continue;
        }

        const uint64_t start = (uint64_t)start_item->valuedouble;
        const uint64_t end = (uint64_t)end_item->valuedouble;
        if (download_state_mark_range_done(state, start, end) != 0) {
            return -1;
        }
    }

    return 0;
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
    free(state->stream_kind);
    free(state->added_through);
    free(state->queue_id);
    free(state->etag);
    free(state->last_modified);
    free(state->done_ranges);
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
    state->filename_inferred = true;
    state->temp_path = temp_path != NULL ? strdup(temp_path) : NULL;
    state->dest_path = dest_path != NULL ? strdup(dest_path) : NULL;
    state->added_through = strdup(AVAR_DL_ADDED_DIRECT);
    state->total_size = total_size;
    state->chunk_size = chunk_size > 0U ? chunk_size : DL_CHUNK_SIZE;

    return state;
}

int download_state_init_chunks(DownloadState *state, const uint64_t total_size,
                               const size_t chunk_size) {
    if (state == NULL || total_size == 0U) {
        return -1;
    }

    state->total_size = total_size;
    state->chunk_size = chunk_size > 0U ? chunk_size : DL_CHUNK_SIZE;
    clip_done_ranges(state);
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
    const cJSON *filename_inferred =
        cJSON_GetObjectItemCaseSensitive(root, AVAR_FIELD_FILENAME_INFERRED);
    if (filename_inferred != NULL && cJSON_IsBool(filename_inferred)) {
        state->filename_inferred = cJSON_IsTrue(filename_inferred);
    } else {
        state->filename_inferred = true;
    }
    state->temp_path = json_get_string(root, AVAR_STATE_FIELD_TEMP_PATH);
    state->dest_path = json_get_string(root, AVAR_STATE_FIELD_DEST_PATH);
    state->status = json_get_string(root, AVAR_FIELD_STATUS);
    state->proxy = json_get_string(root, AVAR_FIELD_PROXY);
    state->queued_at = json_get_string(root, AVAR_FIELD_QUEUED_AT);
    state->last_try_at = json_get_string(root, AVAR_FIELD_LAST_TRY_AT);
    state->description = json_get_string(root, AVAR_FIELD_DESCRIPTION);
    state->original_page = json_get_string(root, AVAR_FIELD_ORIGINAL_PAGE);
    state->referer = json_get_string(root, AVAR_FIELD_REFERER);
    state->stream_kind = json_get_string(root, AVAR_FIELD_STREAM_KIND);
    state->added_through = json_get_string(root, AVAR_FIELD_ADDED_THROUGH);
    state->queue_id = json_get_string(root, AVAR_FIELD_QUEUE_ID);
    state->etag = json_get_string(root, AVAR_FIELD_ETAG);
    state->last_modified = json_get_string(root, AVAR_FIELD_LAST_MODIFIED);

    state->total_size = json_get_u64(root, AVAR_FIELD_TOTAL_BYTES);
    if (state->total_size == 0U) {
        state->total_size = json_get_u64(root, AVAR_STATE_FIELD_TOTAL_SIZE);
    }
    state->bytes_downloaded = json_get_u64(root, AVAR_FIELD_BYTES_DOWNLOADED);

    const cJSON *chunk_size = cJSON_GetObjectItemCaseSensitive(root, AVAR_STATE_FIELD_CHUNK_SIZE);
    if (chunk_size != NULL && cJSON_IsNumber(chunk_size)) {
        state->chunk_size = (size_t)chunk_size->valuedouble;
    } else {
        state->chunk_size = DL_CHUNK_SIZE;
    }
    if (state->chunk_size == 0U) {
        state->chunk_size = DL_CHUNK_SIZE;
    }

    const cJSON *done_ranges =
            cJSON_GetObjectItemCaseSensitive(root, AVAR_STATE_FIELD_DONE_RANGES);
    if (done_ranges != NULL && cJSON_IsArray(done_ranges)) {
        if (load_done_ranges(state, done_ranges) != 0) {
            cJSON_Delete(root);
            download_state_free(state);
            return NULL;
        }
    } else {
        size_t chunk_count = 0U;
        const cJSON *chunk_count_item =
                cJSON_GetObjectItemCaseSensitive(root, AVAR_STATE_FIELD_CHUNK_COUNT);
        if (chunk_count_item != NULL && cJSON_IsNumber(chunk_count_item)) {
            chunk_count = (size_t)chunk_count_item->valuedouble;
        } else if (state->total_size > 0U) {
            chunk_count = download_state_segment_count(state);
        }

        const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(root, AVAR_FIELD_CHUNKS);
        if (load_legacy_chunks(state, chunks, chunk_count) != 0) {
            cJSON_Delete(root);
            download_state_free(state);
            return NULL;
        }
    }

    clip_done_ranges(state);
    cJSON_Delete(root);

    if (state->url == NULL || state->filename == NULL || state->dest_path == NULL) {
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

    if (!download_io_state_write_allowed(path)) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return -1;
    }

    json_add_string_or_null(root, AVAR_FIELD_ID, state->id);
    json_add_string_or_null(root, AVAR_FIELD_URL, state->url);
    json_add_string_or_null(root, AVAR_FIELD_FILENAME, state->filename);
    cJSON_AddBoolToObject(root, AVAR_FIELD_FILENAME_INFERRED, state->filename_inferred);
    json_add_string_or_null(root, AVAR_FIELD_STATUS, state->status);
    json_add_string_or_null(root, AVAR_FIELD_PROXY, state->proxy);
    cJSON_AddNumberToObject(root, AVAR_FIELD_BYTES_DOWNLOADED, (double)state->bytes_downloaded);
    cJSON_AddNumberToObject(root, AVAR_FIELD_TOTAL_BYTES, (double)state->total_size);
    json_add_string_or_null(root, AVAR_FIELD_QUEUED_AT, state->queued_at);
    json_add_string_or_null(root, AVAR_FIELD_LAST_TRY_AT, state->last_try_at);
    json_add_string_or_null(root, AVAR_FIELD_DESCRIPTION, state->description);
    json_add_string_or_null(root, AVAR_FIELD_ORIGINAL_PAGE, state->original_page);
    json_add_string_or_null(root, AVAR_FIELD_REFERER, state->referer);
    json_add_string_or_null(root, AVAR_FIELD_STREAM_KIND, state->stream_kind);
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

    cJSON_AddNumberToObject(root, AVAR_STATE_FIELD_CHUNK_SIZE, (double)state->chunk_size);

    cJSON *ranges = cJSON_AddArrayToObject(root, AVAR_STATE_FIELD_DONE_RANGES);
    if (ranges == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    for (size_t i = 0U; i < state->done_range_count; i++) {
        cJSON *pair = cJSON_CreateArray();
        if (pair == NULL) {
            cJSON_Delete(root);
            return -1;
        }
        cJSON_AddItemToArray(pair, cJSON_CreateNumber((double)state->done_ranges[i].start));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber((double)state->done_ranges[i].end));
        cJSON_AddItemToArray(ranges, pair);
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

uint64_t download_state_bytes_done(const DownloadState *state) {
    if (state == NULL) {
        return 0U;
    }

    if (state->done_range_count == 0U) {
        return state->bytes_downloaded;
    }

    uint64_t bytes = 0U;
    for (size_t i = 0U; i < state->done_range_count; i++) {
        bytes += state->done_ranges[i].end - state->done_ranges[i].start + 1U;
    }
    return bytes;
}

bool download_state_all_chunks_done(const DownloadState *state) {
    if (state == NULL || state->total_size == 0U) {
        return false;
    }

    return download_state_is_range_done(state, 0U, state->total_size - 1U);
}
