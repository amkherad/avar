#include <cJSON.h>

#include <avar.h>
#include <config.h>
#include <logger.h>
#include <queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int queue_find_index(const char *field, const char *value);
static char *queue_generate_id(void);
static char *queue_build_json(const char *id, const char *name, const QueueOptions *options);
static QueueError queue_detach_items(const char *queue_id);
static QueueError queue_purge_items(const char *queue_id);

static int queue_find_index(const char *field, const char *value) {
    if (field == NULL || value == NULL) {
        return -1;
    }

    const size_t count = get_config_array_size(AVAR_CFG_DM_QUEUES);
    for (size_t i = 0; i < count; i++) {
        char *current = get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, field);
        if (current == NULL) {
            continue;
        }

        const bool match = strcmp(current, value) == 0;
        free(current);
        if (match) {
            return (int)i;
        }
    }

    return -1;
}

static char *queue_generate_id(void) {
    char generated[AVAR_QUEUE_ID_BUF_SIZE];
    snprintf(generated, sizeof generated, "%s%llu", AVAR_QUEUE_ID_PREFIX,
             (unsigned long long)time(NULL));

    if (queue_find_index(AVAR_FIELD_ID, generated) >= 0) {
        snprintf(generated, sizeof generated, "%s%llu-%u", AVAR_QUEUE_ID_PREFIX,
                 (unsigned long long)time(NULL), (unsigned)(rand() & 0xffffU));
    }

    return strdup(generated);
}

static char *queue_build_json(const char *id, const char *name, const QueueOptions *options) {
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(obj, AVAR_FIELD_ID, id);
    cJSON_AddStringToObject(obj, AVAR_QUEUE_FIELD_NAME, name);
    cJSON_AddBoolToObject(obj, AVAR_QUEUE_FIELD_STARTED, false);

    if (options != NULL && options->description != NULL) {
        cJSON_AddStringToObject(obj, AVAR_FIELD_DESCRIPTION, options->description);
    } else {
        cJSON_AddNullToObject(obj, AVAR_FIELD_DESCRIPTION);
    }

    if (options != NULL && options->max_concurrent_downloads > 0) {
        cJSON_AddNumberToObject(obj, AVAR_QUEUE_FIELD_MAX_CONCURRENT,
                                (double)options->max_concurrent_downloads);
    } else {
        cJSON_AddNullToObject(obj, AVAR_QUEUE_FIELD_MAX_CONCURRENT);
    }

    if (options != NULL && options->max_connections > 0) {
        cJSON_AddNumberToObject(obj, AVAR_QUEUE_FIELD_MAX_CONNECTIONS,
                                (double)options->max_connections);
    } else {
        cJSON_AddNullToObject(obj, AVAR_QUEUE_FIELD_MAX_CONNECTIONS);
    }

    if (options != NULL && options->max_retries > 0) {
        cJSON_AddNumberToObject(obj, AVAR_QUEUE_FIELD_MAX_RETRIES, (double)options->max_retries);
    } else {
        cJSON_AddNullToObject(obj, AVAR_QUEUE_FIELD_MAX_RETRIES);
    }

    if (options != NULL && options->temp_path != NULL && options->temp_path[0] != '\0') {
        cJSON_AddStringToObject(obj, AVAR_QUEUE_FIELD_TEMP_PATH, options->temp_path);
    } else {
        cJSON_AddNullToObject(obj, AVAR_QUEUE_FIELD_TEMP_PATH);
    }

    if (options != NULL && options->download_path != NULL && options->download_path[0] != '\0') {
        cJSON_AddStringToObject(obj, AVAR_QUEUE_FIELD_DOWNLOAD_PATH, options->download_path);
    } else {
        cJSON_AddNullToObject(obj, AVAR_QUEUE_FIELD_DOWNLOAD_PATH);
    }

    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json;
}

static QueueError queue_detach_items(const char *queue_id) {
    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; i++) {
        char *item_queue = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_QUEUE_ID);
        if (item_queue == NULL || strcmp(item_queue, queue_id) != 0) {
            free(item_queue);
            continue;
        }
        free(item_queue);

        char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, i);
        if (json == NULL) {
            return QueueErrorPersist;
        }

        cJSON *obj = cJSON_Parse(json);
        cJSON_free(json);
        if (obj == NULL || !cJSON_IsObject(obj)) {
            cJSON_Delete(obj);
            return QueueErrorPersist;
        }

        cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_FIELD_QUEUE_ID, cJSON_CreateNull());
        char *updated = cJSON_PrintUnformatted(obj);
        cJSON_Delete(obj);
        if (updated == NULL) {
            return QueueErrorPersist;
        }

        if (replace_config_array_item_at(AVAR_CFG_DM_ITEMS, i, updated) != 0) {
            cJSON_free(updated);
            return QueueErrorPersist;
        }

        cJSON_free(updated);
    }

    return QueueErrorNone;
}

static QueueError queue_purge_items(const char *queue_id) {
    const int removed = remove_config_array_items_where(AVAR_CFG_DM_ITEMS, AVAR_FIELD_QUEUE_ID,
                                                        queue_id);
    if (removed < 0) {
        return QueueErrorPersist;
    }

    return QueueErrorNone;
}

char *queue_resolve_id(const char *id_or_name, const bool by_name) {
    if (id_or_name == NULL || id_or_name[0] == '\0') {
        return NULL;
    }

    const char *field = by_name ? AVAR_QUEUE_FIELD_NAME : AVAR_FIELD_ID;
    const int index = queue_find_index(field, id_or_name);
    if (index < 0) {
        return NULL;
    }

    return get_config_array_item_field(AVAR_CFG_DM_QUEUES, (size_t)index, AVAR_FIELD_ID);
}

QueueError queue_add(const char *name, const QueueOptions *options, char **id_out) {
    if (name == NULL || name[0] == '\0' || id_out == NULL) {
        return QueueErrorInvalidArg;
    }

    if (strlen(name) >= AVAR_QUEUE_NAME_MAX) {
        return QueueErrorInvalidArg;
    }

    if (queue_find_index(AVAR_QUEUE_FIELD_NAME, name) >= 0) {
        return QueueErrorDuplicateName;
    }

    char *id = queue_generate_id();
    if (id == NULL) {
        return QueueErrorPersist;
    }

    char *json = queue_build_json(id, name, options);
    if (json == NULL) {
        free(id);
        return QueueErrorPersist;
    }

    if (append_config_array_item(AVAR_CFG_DM_QUEUES, json) != 0) {
        cJSON_free(json);
        free(id);
        return QueueErrorPersist;
    }

    cJSON_free(json);
    *id_out = id;
    LOG_INFO("Created queue '%s' (%s)", name, id);
    return QueueErrorNone;
}

QueueError queue_remove(const char *id_or_name, const bool by_name, const bool purge_items) {
    if (id_or_name == NULL || id_or_name[0] == '\0') {
        return QueueErrorInvalidArg;
    }

    char *resolved_id = queue_resolve_id(id_or_name, by_name);
    if (resolved_id == NULL) {
        return QueueErrorNotFound;
    }

    const QueueError item_rc = purge_items ? queue_purge_items(resolved_id)
                                           : queue_detach_items(resolved_id);
    if (item_rc != QueueErrorNone) {
        free(resolved_id);
        return item_rc;
    }

    if (remove_config_array_item(AVAR_CFG_DM_QUEUES, AVAR_FIELD_ID, resolved_id) != 0) {
        free(resolved_id);
        return QueueErrorNotFound;
    }

    LOG_INFO("Removed queue %s (%s items %s)", resolved_id, purge_items ? "purged" : "detached",
             purge_items ? "removed" : "kept");
    free(resolved_id);
    return QueueErrorNone;
}

QueueError queue_edit(const char *id, const QueuePatch *patch) {
    if (id == NULL || id[0] == '\0' || patch == NULL) {
        return QueueErrorInvalidArg;
    }

    const int index = queue_find_index(AVAR_FIELD_ID, id);
    if (index < 0) {
        return QueueErrorNotFound;
    }

    char *json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, (size_t)index);
    if (json == NULL) {
        return QueueErrorPersist;
    }

    cJSON *obj = cJSON_Parse(json);
    cJSON_free(json);
    if (obj == NULL || !cJSON_IsObject(obj)) {
        cJSON_Delete(obj);
        return QueueErrorPersist;
    }

    if (patch->set_description) {
        if (patch->description != NULL) {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_FIELD_DESCRIPTION,
                                                   cJSON_CreateString(patch->description));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_FIELD_DESCRIPTION, cJSON_CreateNull());
        }
    }

    if (patch->set_max_concurrent_downloads) {
        if (patch->max_concurrent_downloads > 0) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                obj, AVAR_QUEUE_FIELD_MAX_CONCURRENT,
                cJSON_CreateNumber((double)patch->max_concurrent_downloads));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_MAX_CONCURRENT,
                                                   cJSON_CreateNull());
        }
    }

    if (patch->set_max_connections) {
        if (patch->max_connections > 0) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                obj, AVAR_QUEUE_FIELD_MAX_CONNECTIONS,
                cJSON_CreateNumber((double)patch->max_connections));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_MAX_CONNECTIONS,
                                                   cJSON_CreateNull());
        }
    }

    if (patch->set_max_retries) {
        if (patch->max_retries > 0) {
            cJSON_ReplaceItemInObjectCaseSensitive(
                obj, AVAR_QUEUE_FIELD_MAX_RETRIES,
                cJSON_CreateNumber((double)patch->max_retries));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_MAX_RETRIES,
                                                   cJSON_CreateNull());
        }
    }

    if (patch->set_temp_path) {
        if (patch->temp_path != NULL && patch->temp_path[0] != '\0') {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_TEMP_PATH,
                                                   cJSON_CreateString(patch->temp_path));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_TEMP_PATH,
                                                   cJSON_CreateNull());
        }
    }

    if (patch->set_download_path) {
        if (patch->download_path != NULL && patch->download_path[0] != '\0') {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_DOWNLOAD_PATH,
                                                   cJSON_CreateString(patch->download_path));
        } else {
            cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_DOWNLOAD_PATH,
                                                   cJSON_CreateNull());
        }
    }

    char *updated = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (updated == NULL) {
        return QueueErrorPersist;
    }

    if (replace_config_array_item_at(AVAR_CFG_DM_QUEUES, (size_t)index, updated) != 0) {
        cJSON_free(updated);
        return QueueErrorPersist;
    }

    cJSON_free(updated);
    LOG_INFO("Updated queue %s", id);
    return QueueErrorNone;
}

QueueError queue_start(const char *id) {
    if (id == NULL || id[0] == '\0') {
        return QueueErrorInvalidArg;
    }

    const int index = queue_find_index(AVAR_FIELD_ID, id);
    if (index < 0) {
        return QueueErrorNotFound;
    }

    char *name = get_config_array_item_field(AVAR_CFG_DM_QUEUES, (size_t)index,
                                             AVAR_QUEUE_FIELD_NAME);
    LOG_INFO("Scheduling queue '%s' (%s)", name != NULL ? name : id, id);
    free(name);

    char *json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, (size_t)index);
    if (json == NULL) {
        return QueueErrorPersist;
    }

    cJSON *obj = cJSON_Parse(json);
    cJSON_free(json);
    if (obj == NULL || !cJSON_IsObject(obj)) {
        cJSON_Delete(obj);
        return QueueErrorPersist;
    }

    cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_QUEUE_FIELD_STARTED, cJSON_CreateTrue());
    char *updated = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (updated == NULL) {
        return QueueErrorPersist;
    }

    if (replace_config_array_item_at(AVAR_CFG_DM_QUEUES, (size_t)index, updated) != 0) {
        cJSON_free(updated);
        return QueueErrorPersist;
    }

    cJSON_free(updated);
    return QueueErrorNone;
}

bool queue_is_started(const char *id) {
    if (id == NULL || id[0] == '\0') {
        return false;
    }

    const int index = queue_find_index(AVAR_FIELD_ID, id);
    if (index < 0) {
        return false;
    }

    char *json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, (size_t)index);
    if (json == NULL) {
        return false;
    }

    cJSON *obj = cJSON_Parse(json);
    cJSON_free(json);
    if (obj == NULL || !cJSON_IsObject(obj)) {
        cJSON_Delete(obj);
        return false;
    }

    const cJSON *started = cJSON_GetObjectItemCaseSensitive(obj, AVAR_QUEUE_FIELD_STARTED);
    const bool running = cJSON_IsTrue(started);
    cJSON_Delete(obj);
    return running;
}

QueueError queue_stop(const char *id) {
    if (id == NULL || id[0] == '\0') {
        return QueueErrorInvalidArg;
    }

    if (queue_find_index(AVAR_FIELD_ID, id) < 0) {
        return QueueErrorNotFound;
    }

    char *name = NULL;
    const int queue_index = queue_find_index(AVAR_FIELD_ID, id);
    if (queue_index >= 0) {
        name = get_config_array_item_field(AVAR_CFG_DM_QUEUES, (size_t)queue_index,
                                           AVAR_QUEUE_FIELD_NAME);
    }

    LOG_INFO("Stopping downloads in queue '%s' (%s)", name != NULL ? name : id, id);
    free(name);

    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; i++) {
        char *item_queue = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_QUEUE_ID);
        if (item_queue == NULL || strcmp(item_queue, id) != 0) {
            free(item_queue);
            continue;
        }
        free(item_queue);

        char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_STATUS);
        if (status == NULL || strcmp(status, AVAR_DL_STATUS_DOWNLOADING) != 0) {
            free(status);
            continue;
        }
        free(status);

        char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, i);
        if (json == NULL) {
            return QueueErrorPersist;
        }

        cJSON *obj = cJSON_Parse(json);
        cJSON_free(json);
        if (obj == NULL || !cJSON_IsObject(obj)) {
            cJSON_Delete(obj);
            return QueueErrorPersist;
        }

        cJSON_ReplaceItemInObjectCaseSensitive(obj, AVAR_FIELD_STATUS,
                                               cJSON_CreateString(AVAR_DL_STATUS_QUEUED));
        char *updated = cJSON_PrintUnformatted(obj);
        cJSON_Delete(obj);
        if (updated == NULL) {
            return QueueErrorPersist;
        }

        if (replace_config_array_item_at(AVAR_CFG_DM_ITEMS, i, updated) != 0) {
            cJSON_free(updated);
            return QueueErrorPersist;
        }

        cJSON_free(updated);
    }

    char *queue_json = get_config_array_item_json(AVAR_CFG_DM_QUEUES, (size_t)queue_index);
    if (queue_json != NULL) {
        cJSON *queue_obj = cJSON_Parse(queue_json);
        cJSON_free(queue_json);
        if (queue_obj != NULL && cJSON_IsObject(queue_obj)) {
            cJSON_ReplaceItemInObjectCaseSensitive(queue_obj, AVAR_QUEUE_FIELD_STARTED,
                                                   cJSON_CreateFalse());
            char *updated_queue = cJSON_PrintUnformatted(queue_obj);
            cJSON_Delete(queue_obj);
            if (updated_queue != NULL) {
                (void)replace_config_array_item_at(AVAR_CFG_DM_QUEUES, (size_t)queue_index,
                                                  updated_queue);
                cJSON_free(updated_queue);
            }
        } else {
            cJSON_Delete(queue_obj);
        }
    }

    return QueueErrorNone;
}

size_t queue_count(void) {
    return get_config_array_size(AVAR_CFG_DM_QUEUES);
}

char *queue_id_at(const size_t index) {
    return get_config_array_item_field(AVAR_CFG_DM_QUEUES, index, AVAR_FIELD_ID);
}

char *queue_name_at(const size_t index) {
    return get_config_array_item_field(AVAR_CFG_DM_QUEUES, index, AVAR_QUEUE_FIELD_NAME);
}
