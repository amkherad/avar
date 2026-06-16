#include <instance.h>

#include <avar.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool instance_id_is_valid(const char *id) {
    if (id == NULL || id[0] == '\0') {
        return false;
    }

    for (size_t i = 0; id[i] != '\0'; ++i) {
        const unsigned char ch = (unsigned char)id[i];
        if (isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.') {
            continue;
        }
        return false;
    }

    return strlen(id) <= AVAR_INSTANCE_ID_MAX;
}

static const char *instance_id_from_env(void) {
    const char *value = getenv(AVAR_ENV_INSTANCE);
    if (value == NULL || value[0] == '\0') {
        return NULL;
    }
    if (!instance_id_is_valid(value)) {
        return NULL;
    }
    return value;
}

bool avar_instance_configured(void) {
    return instance_id_from_env() != NULL;
}

bool avar_instance_id(char *out, size_t out_size) {
    if (out == NULL || out_size == 0) {
        return false;
    }

    const char *value = instance_id_from_env();
    if (value == NULL) {
        out[0] = '\0';
        return false;
    }

    snprintf(out, out_size, "%s", value);
    return true;
}

const char *avar_app_name(void) {
    static char app_name[sizeof(APP_ID) + 1U + AVAR_INSTANCE_ID_MAX];

    const char *instance = instance_id_from_env();
    if (instance == NULL) {
        return APP_ID;
    }

    snprintf(app_name, sizeof app_name, "%s-%s", APP_ID, instance);
    return app_name;
}

static bool insert_instance_suffix(char *out, size_t out_size, const char *base, size_t insert_at) {
    char instance[AVAR_INSTANCE_ID_MAX + 1U];
    if (!avar_instance_id(instance, sizeof instance)) {
        snprintf(out, out_size, "%s", base);
        return true;
    }

    const int written = snprintf(out, out_size, "%.*s-%s%s", (int)insert_at, base, instance, base + insert_at);
    return written > 0 && (size_t)written < out_size;
}

bool avar_path_with_instance(char *out, size_t out_size, const char *path) {
    if (out == NULL || out_size == 0 || path == NULL) {
        return false;
    }

    const char *separator = strrchr(path, PATH_SEPARATOR);
#if defined(_WIN32)
    const char *alt_separator = strrchr(path, '/');
    if (alt_separator != NULL && (separator == NULL || alt_separator > separator)) {
        separator = alt_separator;
    }
#endif
    const char *basename = separator != NULL ? separator + 1 : path;
    const char *dot = strrchr(basename, '.');
    const size_t insert_at = dot != NULL ? (size_t)(dot - path) : strlen(path);

    return insert_instance_suffix(out, out_size, path, insert_at);
}

bool avar_named_resource_with_instance(char *out, size_t out_size, const char *name) {
    if (out == NULL || out_size == 0 || name == NULL) {
        return false;
    }

    return insert_instance_suffix(out, out_size, name, strlen(name));
}

uint16_t avar_instance_port_offset(void) {
    char instance[AVAR_INSTANCE_ID_MAX + 1U];
    if (!avar_instance_id(instance, sizeof instance)) {
        return 0U;
    }

    uint32_t hash = 2166136261U;
    for (size_t i = 0; instance[i] != '\0'; ++i) {
        hash ^= (uint8_t)instance[i];
        hash *= 16777619U;
    }

    return (uint16_t)((hash % 999U) + 1U);
}
