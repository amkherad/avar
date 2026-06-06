#include <config.h>
#include <stdio.h>
#include <file-system.h>
#include <stdlib.h>
#include <string.h>

#include <avar.h>

struct ConfigContext {
    bool initialized;

    FILE *_configFile;
    stringa dir;
    stringa path;
};

struct ConfigPaths {
    stringa dir;
    stringa file;
};

static struct ConfigContext _config;

static struct ConfigPaths get_config_path(bool user);

void ensure_initialized() {
    if (_config.initialized) {
        return;
    }

    LOG_DEBUG("Initializing config API, trying to infer config path");

    auto configPaths = get_config_path(true);
    if (!configPaths.file) {
        fatal("Failed to infer config path");
    }

    LOG_DEBUG("Using config path: %s", configPaths.file);
    _config.path = configPaths.file;
    _config.dir = configPaths.dir;

    if (make_dirs_in_path(configPaths.dir) != 0) {
        fatal("Failed to create config path");
    }


    _config._configFile = fopen(configPaths.file, "w+");
    if (!_config._configFile) {
        if (is_log_level(LOG_LEVEL_DEBUG)) {
            perror("fread");
        }
        fatal("Failed to open config file for reading");
    }

    _config.initialized = true;
}

string get_config_or_default(stringa key, string defaultValue) {
    string value = get_config(key);
    if (value == nullptr) {
        return defaultValue;
    }
    return value;
}

string get_config(stringa key) {
    ensure_initialized();
    (void) key;
    return nullptr;
}

void set_config(stringa key, string value) {
    ensure_initialized();
    (void) key;
    (void) value;
}

void save_config(string path) {
    ensure_initialized();
    (void) path;
}

static struct ConfigPaths get_config_path(bool user) {
    (void) user;
    const int PATH_BUFFER_SIZE = 256;

    auto path = config_path(APP_ID);
    auto len = strlen(path);
    if (len > PATH_BUFFER_SIZE) {
        LOG_FATAL("Paths longer that %s in length are not supported", PATH_BUFFER_SIZE);
        fatal(nullptr);
    }

    auto buffer_size = len + sizeof(PATH_SEPARATOR) + sizeof("config.json") + 1;
    auto buffer = malloc(buffer_size);
    snprintf(buffer, buffer_size, "%s%c%s", path, PATH_SEPARATOR, "config.json");

    const struct ConfigPaths result = {
        .dir = path,
        .file = buffer
    };
    return result;
}
