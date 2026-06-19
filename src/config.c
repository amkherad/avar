#include <cJSON.h>

#include <config.h>
#include <file-system.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#include <avar.h>
#include <instance.h>
#include <logger.h>
#include <utils.h>

struct ConfigContext {
    bool initialized;
    cJSON *root;
    char *dir;
    char *path;
};

struct ConfigPaths {
    char *dir;
    char *file;
};

static struct ConfigContext _config = {0};

#if defined(_WIN32)
static CRITICAL_SECTION g_config_lock;
static bool g_config_lock_ready = false;

static void config_lock_init(void) {
    if (!g_config_lock_ready) {
        InitializeCriticalSection(&g_config_lock);
        g_config_lock_ready = true;
    }
}

static void config_lock(void) {
    config_lock_init();
    EnterCriticalSection(&g_config_lock);
}

static void config_unlock(void) {
    LeaveCriticalSection(&g_config_lock);
}
#else
static pthread_mutex_t g_config_lock = PTHREAD_MUTEX_INITIALIZER;

static void config_lock(void) {
    (void)pthread_mutex_lock(&g_config_lock);
}

static void config_unlock(void) {
    (void)pthread_mutex_unlock(&g_config_lock);
}
#endif

static char *get_config_from_env(stringa key);
static struct ConfigPaths get_config_path(bool user);
static void ensure_initialized(void);
static int persist_config(const char *path);
static cJSON *resolve_parent(cJSON *root, const char *key, bool create, char **leaf_out);
static void prune_empty_parents(cJSON *root, const char *key);

static struct ConfigPaths get_config_path(bool user) {
    (void)user;
    const size_t path_buffer_size = AVAR_CONFIG_PATH_MAX;

    char *dir = config_path(avar_app_name());
    if (dir == NULL) {
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    const size_t len = strlen(dir);
    if (len > path_buffer_size) {
        LOG_FATAL("Paths longer than %zu are not supported", path_buffer_size);
        free(dir);
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    const size_t file_len = len + sizeof(PATH_SEPARATOR) + sizeof(AVAR_CONFIG_FILENAME);
    char *file = malloc(file_len);
    if (file == NULL) {
        free(dir);
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    snprintf(file, file_len, "%s%c%s", dir, PATH_SEPARATOR, AVAR_CONFIG_FILENAME);
    return (struct ConfigPaths){.dir = dir, .file = file};
}

static cJSON *load_json_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long size = ftell(file);
    if (size < 0) {
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

    return root;
}

static void ensure_initialized(void) {
    if (_config.initialized) {
        return;
    }

    LOG_DEBUG("Initializing config API, trying to infer config path");

    const struct ConfigPaths config_paths = get_config_path(true);
    if (config_paths.file == NULL) {
        fatal("Failed to infer config path");
    }

    LOG_DEBUG("Using config path: %s", config_paths.file);
    _config.path = config_paths.file;
    _config.dir = config_paths.dir;

    if (make_dirs_in_path(_config.dir) != 0) {
        fatal("Failed to create config directory");
    }

    _config.root = load_json_file(_config.path);
    if (_config.root == NULL) {
        _config.root = cJSON_CreateObject();
        if (_config.root == NULL) {
            fatal("Failed to allocate config root");
        }
    }

    _config.initialized = true;
}

static int persist_config(const char *path) {
    if (_config.root == NULL || path == NULL) {
        return -1;
    }

    char *json = cJSON_Print(_config.root);
    if (json == NULL) {
        LOG_ERROR("Failed to serialize config");
        return -1;
    }

    const size_t path_len = strlen(path);
    char *tmp_path = malloc(path_len + sizeof(AVAR_CONFIG_TMP_SUFFIX));
    if (tmp_path == NULL) {
        cJSON_free(json);
        LOG_ERROR("Failed to allocate temporary config path");
        return -1;
    }

    snprintf(tmp_path, path_len + sizeof(AVAR_CONFIG_TMP_SUFFIX), "%s%s", path,
             AVAR_CONFIG_TMP_SUFFIX);

    FILE *file = fopen(tmp_path, "wb");
    if (file == NULL) {
        free(tmp_path);
        cJSON_free(json);
        LOG_ERROR("Failed to open config file for writing: %s", tmp_path);
        return -1;
    }

    const size_t json_len = strlen(json);
    const size_t written = fwrite(json, 1, json_len, file);
    const int flush_rc = fflush(file);
#if !defined(_WIN32)
    fsync(fileno(file));
#endif
    fclose(file);
    cJSON_free(json);

    if (written != json_len || flush_rc != 0) {
        remove(tmp_path);
        free(tmp_path);
        LOG_ERROR("Failed to write config file: %s", tmp_path);
        return -1;
    }

#if defined(_WIN32)
    remove(path);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        LOG_ERROR("Failed to replace config file: %s", path);
        return -1;
    }

    free(tmp_path);
    return 0;
}

static cJSON *resolve_parent(cJSON *root, const char *key, const bool create, char **leaf_out) {
    if (root == NULL || key == NULL || leaf_out == NULL || key[0] == '\0') {
        return NULL;
    }

    char *mutable_key = strdup(key);
    if (mutable_key == NULL) {
        return NULL;
    }

    cJSON *current = root;
    char *segment = mutable_key;

    for (;;) {
        char *dot = strchr(segment, '.');
        if (dot != NULL) {
            *dot = '\0';
        }

        if (segment[0] == '\0') {
            free(mutable_key);
            return NULL;
        }

        if (dot == NULL) {
            *leaf_out = strdup(segment);
            free(mutable_key);
            if (*leaf_out == NULL) {
                return NULL;
            }
            return current;
        }

        cJSON *child = cJSON_GetObjectItemCaseSensitive(current, segment);
        if (child == NULL) {
            if (!create) {
                free(mutable_key);
                return NULL;
            }
            child = cJSON_AddObjectToObject(current, segment);
            if (child == NULL) {
                free(mutable_key);
                return NULL;
            }
        } else if (!cJSON_IsObject(child)) {
            if (!create) {
                free(mutable_key);
                return NULL;
            }
            cJSON_DeleteItemFromObjectCaseSensitive(current, segment);
            child = cJSON_AddObjectToObject(current, segment);
            if (child == NULL) {
                free(mutable_key);
                return NULL;
            }
        }

        current = child;
        segment = dot + 1;
    }
}

static void prune_empty_parents(cJSON *root, const char *key) {
    char *path = strdup(key);
    if (path == NULL) {
        return;
    }

    for (;;) {
        char *last_dot = strrchr(path, '.');
        if (last_dot == NULL) {
            break;
        }

        *last_dot = '\0';

        char *leaf = NULL;
        cJSON *parent = resolve_parent(root, path, false, &leaf);
        if (parent == NULL || leaf == NULL) {
            free(leaf);
            break;
        }

        cJSON *node = cJSON_GetObjectItemCaseSensitive(parent, leaf);
        free(leaf);

        if (node == NULL) {
            continue;
        }

        if (!cJSON_IsObject(node) || cJSON_GetArraySize(node) != 0) {
            break;
        }

        char *parent_leaf = NULL;
        cJSON *grandparent = resolve_parent(root, path, false, &parent_leaf);
        if (grandparent == NULL || parent_leaf == NULL) {
            free(parent_leaf);
            break;
        }

        cJSON_DeleteItemFromObjectCaseSensitive(grandparent, parent_leaf);
        free(parent_leaf);
    }

    free(path);
}

static char *get_config_from_env(stringa key) {
    if (key == NULL) {
        return NULL;
    }

    char env_name[AVAR_CONFIG_PATH_MAX];
    const int written = snprintf(env_name, sizeof env_name, "%s%s", AVAR_ENV_PREFIX, key);
    if (written < 0 || (size_t)written >= sizeof env_name) {
        return NULL;
    }

    const char *value = getenv(env_name);
    if (value == NULL) {
        return NULL;
    }

    return strdup(value);
}

char *get_config(stringa key) {
    char *env_value = get_config_from_env(key);
    if (env_value != NULL) {
        return env_value;
    }

    config_lock();
    ensure_initialized();

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, false, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        config_unlock();
        return NULL;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    free(leaf);

    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL) {
        config_unlock();
        return NULL;
    }

    char *value = strdup(item->valuestring);
    config_unlock();
    return value;
}

char *get_config_or_default(stringa key, stringa default_value) {
    char *value = get_config(key);
    if (value != NULL) {
        return value;
    }

    if (default_value == NULL) {
        return NULL;
    }

    return strdup(default_value);
}

int set_config(stringa key, stringa value) {
    config_lock();
    ensure_initialized();

    if (key == NULL || value == NULL) {
        config_unlock();
        LOG_ERROR("Config key and value must not be NULL");
        return -1;
    }

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, true, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        config_unlock();
        LOG_ERROR("Invalid config key: %s", key);
        return -1;
    }

    cJSON *existing = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    if (existing != NULL) {
        if (!cJSON_IsString(existing)) {
            cJSON_ReplaceItemInObjectCaseSensitive(parent, leaf, cJSON_CreateString(value));
        } else if (cJSON_SetValuestring(existing, value) == NULL) {
            free(leaf);
            config_unlock();
            LOG_ERROR("Failed to set config value for key: %s", key);
            return -1;
        }
    } else if (cJSON_AddStringToObject(parent, leaf, value) == NULL) {
        free(leaf);
        config_unlock();
        LOG_ERROR("Failed to add config value for key: %s", key);
        return -1;
    }

    free(leaf);
    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

int reset_config(stringa key) {
    config_lock();
    ensure_initialized();

    if (key == NULL) {
        config_unlock();
        LOG_ERROR("Config key must not be NULL");
        return -1;
    }

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, false, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        config_unlock();
        LOG_ERROR("Config key not found: %s", key);
        return -1;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    if (item == NULL) {
        free(leaf);
        config_unlock();
        LOG_ERROR("Config key not found: %s", key);
        return -1;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(parent, leaf);
    free(leaf);
    prune_empty_parents(_config.root, key);

    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

int reset_all_config(void) {
    config_lock();
    ensure_initialized();

    cJSON_Delete(_config.root);
    _config.root = cJSON_CreateObject();
    if (_config.root == NULL) {
        config_unlock();
        LOG_ERROR("Failed to reset config");
        return -1;
    }

    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

int save_config(stringa path) {
    config_lock();
    ensure_initialized();

    if (path == NULL) {
        config_unlock();
        LOG_ERROR("Config path must not be NULL");
        return -1;
    }

    const int rc = persist_config(path);
    config_unlock();
    return rc;
}

int load_config(stringa path) {
    config_lock();
    ensure_initialized();

    if (path == NULL) {
        config_unlock();
        LOG_ERROR("Config path must not be NULL");
        return -1;
    }

    cJSON *loaded = load_json_file(path);
    if (loaded == NULL) {
        config_unlock();
        LOG_ERROR("Failed to load config from: %s", path);
        return -1;
    }

    cJSON_Delete(_config.root);
    _config.root = loaded;
    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

static cJSON *resolve_array_parent(const char *key, bool create, char **leaf_out) {
    if (key == NULL || leaf_out == NULL) {
        return NULL;
    }

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, create, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        return NULL;
    }

    cJSON *array = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    if (array == NULL) {
        if (!create) {
            free(leaf);
            return NULL;
        }
        array = cJSON_AddArrayToObject(parent, leaf);
        if (array == NULL) {
            free(leaf);
            return NULL;
        }
    } else if (!cJSON_IsArray(array)) {
        if (!create) {
            free(leaf);
            return NULL;
        }
        cJSON_ReplaceItemInObjectCaseSensitive(parent, leaf, cJSON_CreateArray());
        array = cJSON_GetObjectItemCaseSensitive(parent, leaf);
        if (array == NULL) {
            free(leaf);
            return NULL;
        }
    }

    *leaf_out = leaf;
    return array;
}

size_t get_config_array_size(const char *key) {
    config_lock();
    ensure_initialized();

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        config_unlock();
        return 0;
    }

    const size_t size = (size_t)cJSON_GetArraySize(array);
    config_unlock();
    return size;
}

char *get_config_array_item_field(const char *key, const size_t index, const char *field) {
    config_lock();
    ensure_initialized();

    if (field == NULL) {
        config_unlock();
        return NULL;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        config_unlock();
        return NULL;
    }

    const cJSON *item = cJSON_GetArrayItem(array, (int)index);
    if (item == NULL || !cJSON_IsObject(item)) {
        config_unlock();
        return NULL;
    }

    const cJSON *value = cJSON_GetObjectItemCaseSensitive(item, field);
    if (value == NULL) {
        config_unlock();
        return NULL;
    }

    char *result = NULL;
    if (cJSON_IsString(value) && value->valuestring != NULL) {
        result = strdup(value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        char buffer[32];
        snprintf(buffer, sizeof buffer, "%.0f", value->valuedouble);
        result = strdup(buffer);
    }

    config_unlock();
    return result;
}

int append_config_array_item(const char *key, const char *json_object) {
    config_lock();
    ensure_initialized();

    if (key == NULL || json_object == NULL) {
        config_unlock();
        return -1;
    }

    cJSON *parsed = cJSON_Parse(json_object);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        config_unlock();
        LOG_ERROR("Invalid JSON object for config array item");
        return -1;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, true, &leaf);
    free(leaf);
    if (array == NULL) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    if (!cJSON_AddItemToArray(array, parsed)) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

int update_config_array_item(const char *key, const char *match_field,
                             const char *match_value, const char *json_object) {
    config_lock();
    ensure_initialized();

    if (key == NULL || match_field == NULL || match_value == NULL || json_object == NULL) {
        config_unlock();
        return -1;
    }

    cJSON *parsed = cJSON_Parse(json_object);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    const int size = cJSON_GetArraySize(array);
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item == NULL || !cJSON_IsObject(item)) {
            continue;
        }

        const cJSON *match = cJSON_GetObjectItemCaseSensitive(item, match_field);
        if (match == NULL || !cJSON_IsString(match) || match->valuestring == NULL) {
            continue;
        }

        if (strcmp(match->valuestring, match_value) != 0) {
            continue;
        }

        cJSON_ReplaceItemInArray(array, i, parsed);
        const int rc = persist_config(_config.path);
        config_unlock();
        return rc;
    }

    cJSON_Delete(parsed);
    config_unlock();
    return -1;
}

int remove_config_array_item(const char *key, const char *match_field, const char *match_value) {
    config_lock();
    ensure_initialized();

    if (key == NULL || match_field == NULL || match_value == NULL) {
        config_unlock();
        return -1;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        config_unlock();
        return -1;
    }

    const int size = cJSON_GetArraySize(array);
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item == NULL || !cJSON_IsObject(item)) {
            continue;
        }

        const cJSON *match = cJSON_GetObjectItemCaseSensitive(item, match_field);
        if (match == NULL || !cJSON_IsString(match) || match->valuestring == NULL) {
            continue;
        }

        if (strcmp(match->valuestring, match_value) != 0) {
            continue;
        }

        cJSON_DeleteItemFromArray(array, i);
        const int rc = persist_config(_config.path);
        config_unlock();
        return rc;
    }

    config_unlock();
    return -1;
}

char *get_config_array_item_json(const char *key, const size_t index) {
    config_lock();
    ensure_initialized();

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        config_unlock();
        return NULL;
    }

    const cJSON *item = cJSON_GetArrayItem(array, (int)index);
    if (item == NULL || !cJSON_IsObject(item)) {
        config_unlock();
        return NULL;
    }

    char *json = cJSON_PrintUnformatted(item);
    config_unlock();
    return json;
}

int replace_config_array_item_at(const char *key, const size_t index, const char *json_object) {
    config_lock();
    ensure_initialized();

    if (key == NULL || json_object == NULL) {
        config_unlock();
        return -1;
    }

    cJSON *parsed = cJSON_Parse(json_object);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    if (!cJSON_ReplaceItemInArray(array, (int)index, parsed)) {
        cJSON_Delete(parsed);
        config_unlock();
        return -1;
    }

    const int rc = persist_config(_config.path);
    config_unlock();
    return rc;
}

int remove_config_array_items_where(const char *key, const char *match_field,
                                    const char *match_value) {
    config_lock();
    ensure_initialized();

    if (key == NULL || match_field == NULL || match_value == NULL) {
        config_unlock();
        return -1;
    }

    char *leaf = NULL;
    cJSON *array = resolve_array_parent(key, false, &leaf);
    free(leaf);
    if (array == NULL) {
        config_unlock();
        return 0;
    }

    int removed = 0;
    for (int i = cJSON_GetArraySize(array) - 1; i >= 0; i--) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item == NULL || !cJSON_IsObject(item)) {
            continue;
        }

        const cJSON *match = cJSON_GetObjectItemCaseSensitive(item, match_field);
        if (match == NULL || !cJSON_IsString(match) || match->valuestring == NULL) {
            continue;
        }

        if (strcmp(match->valuestring, match_value) != 0) {
            continue;
        }

        cJSON_DeleteItemFromArray(array, i);
        removed++;
    }

    int rc = removed;
    if (removed > 0 && persist_config(_config.path) != 0) {
        rc = -1;
    }

    config_unlock();
    return rc;
}

#if defined(AVAR_TESTING)
int config_open_at(const char *config_file) {
    if (config_file == NULL) {
        return -1;
    }

    config_lock();

    if (_config.initialized) {
        cJSON_Delete(_config.root);
        free(_config.path);
        free(_config.dir);
        memset(&_config, 0, sizeof(_config));
    }

    _config.path = strdup(config_file);
    if (_config.path == NULL) {
        config_unlock();
        return -1;
    }

    const char *last_sep = strrchr(config_file, PATH_SEPARATOR);
    if (last_sep != NULL) {
        _config.dir = strndup(config_file, (size_t)(last_sep - config_file));
    } else {
        _config.dir = strdup(".");
    }

    if (_config.dir == NULL) {
        free(_config.path);
        _config.path = NULL;
        config_unlock();
        return -1;
    }

    _config.root = load_json_file(config_file);
    if (_config.root == NULL) {
        _config.root = cJSON_CreateObject();
        if (_config.root == NULL) {
            free(_config.path);
            free(_config.dir);
            _config.path = NULL;
            _config.dir = NULL;
            config_unlock();
            return -1;
        }
    }

    _config.initialized = true;
    config_unlock();
    return 0;
}
#endif
