#include <cJSON.h>

#include <config.h>
#include <file-system.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
    #include <unistd.h>
#endif

#include <avar.h>

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

static struct ConfigPaths get_config_path(bool user);
static void ensure_initialized(void);
static int persist_config(const char *path);
static cJSON *resolve_parent(cJSON *root, const char *key, bool create, char **leaf_out);
static void prune_empty_parents(cJSON *root, const char *key);

static struct ConfigPaths get_config_path(bool user) {
    (void)user;
    const size_t path_buffer_size = 256;

    char *dir = config_path(APP_ID);
    if (dir == NULL) {
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    const size_t len = strlen(dir);
    if (len > path_buffer_size) {
        LOG_FATAL("Paths longer than %zu are not supported", path_buffer_size);
        free(dir);
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    const size_t file_len = len + sizeof(PATH_SEPARATOR) + sizeof("config.json");
    char *file = malloc(file_len);
    if (file == NULL) {
        free(dir);
        return (struct ConfigPaths){.dir = NULL, .file = NULL};
    }

    snprintf(file, file_len, "%s%cconfig.json", dir, PATH_SEPARATOR);
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
    char *tmp_path = malloc(path_len + 5);
    if (tmp_path == NULL) {
        cJSON_free(json);
        LOG_ERROR("Failed to allocate temporary config path");
        return -1;
    }

    snprintf(tmp_path, path_len + 5, "%s.tmp", path);

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

char *get_config(stringa key) {
    ensure_initialized();

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, false, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        return NULL;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    free(leaf);

    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL) {
        return NULL;
    }

    return strdup(item->valuestring);
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
    ensure_initialized();

    if (key == NULL || value == NULL) {
        LOG_ERROR("Config key and value must not be NULL");
        return -1;
    }

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, true, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        LOG_ERROR("Invalid config key: %s", key);
        return -1;
    }

    cJSON *existing = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    if (existing != NULL) {
        if (!cJSON_IsString(existing)) {
            cJSON_ReplaceItemInObjectCaseSensitive(parent, leaf, cJSON_CreateString(value));
        } else if (cJSON_SetValuestring(existing, value) == NULL) {
            free(leaf);
            LOG_ERROR("Failed to set config value for key: %s", key);
            return -1;
        }
    } else if (cJSON_AddStringToObject(parent, leaf, value) == NULL) {
        free(leaf);
        LOG_ERROR("Failed to add config value for key: %s", key);
        return -1;
    }

    free(leaf);
    return persist_config(_config.path);
}

int reset_config(stringa key) {
    ensure_initialized();

    if (key == NULL) {
        LOG_ERROR("Config key must not be NULL");
        return -1;
    }

    char *leaf = NULL;
    cJSON *parent = resolve_parent(_config.root, key, false, &leaf);
    if (parent == NULL || leaf == NULL) {
        free(leaf);
        LOG_ERROR("Config key not found: %s", key);
        return -1;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, leaf);
    if (item == NULL) {
        free(leaf);
        LOG_ERROR("Config key not found: %s", key);
        return -1;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(parent, leaf);
    free(leaf);
    prune_empty_parents(_config.root, key);

    return persist_config(_config.path);
}

int reset_all_config(void) {
    ensure_initialized();

    cJSON_Delete(_config.root);
    _config.root = cJSON_CreateObject();
    if (_config.root == NULL) {
        LOG_ERROR("Failed to reset config");
        return -1;
    }

    return persist_config(_config.path);
}

int save_config(stringa path) {
    ensure_initialized();

    if (path == NULL) {
        LOG_ERROR("Config path must not be NULL");
        return -1;
    }

    return persist_config(path);
}

int load_config(stringa path) {
    ensure_initialized();

    if (path == NULL) {
        LOG_ERROR("Config path must not be NULL");
        return -1;
    }

    cJSON *loaded = load_json_file(path);
    if (loaded == NULL) {
        LOG_ERROR("Failed to load config from: %s", path);
        return -1;
    }

    cJSON_Delete(_config.root);
    _config.root = loaded;
    return persist_config(_config.path);
}
