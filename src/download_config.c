#include <download_config.h>

#include <avar.h>
#include <config.h>

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <string.h>
static int ascii_stricmp(const char *a, const char *b) {
    return _stricmp(a, b);
}
#else
    #include <strings.h>
static int ascii_stricmp(const char *a, const char *b) {
    return strcasecmp(a, b);
}
#endif

static size_t parse_config_size(const char *key, const size_t default_value) {
    char *value = get_config_or_default(key, NULL);
    if (value == NULL) {
        return default_value;
    }

    char *end = NULL;
    const unsigned long long parsed = strtoull(value, &end, 10);
    free(value);

    if (end == value || parsed == 0) {
        return default_value;
    }

    return (size_t)parsed;
}

static bool parse_config_bool(const char *key, const bool default_value) {
    char *value = get_config_or_default(key, NULL);
    if (value == NULL) {
        return default_value;
    }

    const bool result =
            strcmp(value, "1") == 0 || ascii_stricmp(value, "true") == 0
            || ascii_stricmp(value, "yes") == 0;
    free(value);
    return result;
}

static double parse_config_double(const char *key, const double default_value) {
    char *value = get_config_or_default(key, NULL);
    if (value == NULL) {
        return default_value;
    }

    char *end = NULL;
    const double parsed = strtod(value, &end);
    free(value);

    if (end == value) {
        return default_value;
    }

    return parsed;
}

static size_t queue_connection_limit(const char *queue_id) {
    if (queue_id == NULL) {
        return 0;
    }

    const size_t count = get_config_array_size(AVAR_CFG_DM_QUEUES);
    for (size_t i = 0; i < count; i++) {
        char *id = get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_FIELD_ID);
        if (id == NULL || strcmp(id, queue_id) != 0) {
            free(id);
            continue;
        }
        free(id);

        char *max_connections =
                get_config_array_item_field(AVAR_CFG_DM_QUEUES, i, AVAR_QUEUE_FIELD_MAX_CONNECTIONS);
        if (max_connections == NULL) {
            return 0;
        }

        char *end = NULL;
        const unsigned long long parsed = strtoull(max_connections, &end, 10);
        free(max_connections);

        if (end == max_connections || parsed == 0) {
            return 0;
        }

        return (size_t)parsed;
    }

    return 0;
}

DownloadSegmentConfig download_config_load(const char *queue_id) {
    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = DL_DEFAULT_SEGMENT_CONCURRENCY,
            .chunk_size = DL_CHUNK_SIZE,
            .min_file_size = DL_DEFAULT_MIN_SEGMENT_FILE_SIZE,
            .left_heavy_front_ratio = DL_DEFAULT_LEFT_HEAVY_FRONT_RATIO,
    };

    cfg.enabled = parse_config_bool(AVAR_CFG_DM_SEGMENTATION_ENABLED, true);

    char *strategy = get_config_or_default(AVAR_CFG_DM_SEGMENTATION_STRATEGY,
                                           AVAR_SEGMENT_STRATEGY_BALANCED);
    if (strategy != NULL) {
        cfg.strategy = segment_strategy_from_string(strategy);
        free(strategy);
    }

    cfg.concurrency =
            parse_config_size(AVAR_CFG_DM_SEGMENTATION_CONCURRENCY, DL_DEFAULT_SEGMENT_CONCURRENCY);
    cfg.chunk_size = parse_config_size(AVAR_CFG_DM_SEGMENTATION_CHUNK_SIZE, DL_CHUNK_SIZE);
    cfg.min_file_size =
            parse_config_size(AVAR_CFG_DM_SEGMENTATION_MIN_FILE_SIZE, DL_DEFAULT_MIN_SEGMENT_FILE_SIZE);
    cfg.left_heavy_front_ratio = parse_config_double(AVAR_CFG_DM_SEGMENTATION_LEFT_HEAVY_RATIO,
                                                     DL_DEFAULT_LEFT_HEAVY_FRONT_RATIO);

    const size_t queue_limit = queue_connection_limit(queue_id);
    if (queue_limit > 0 && queue_limit < cfg.concurrency) {
        cfg.concurrency = queue_limit;
    }

    if (cfg.concurrency < 1U) {
        cfg.concurrency = 1U;
    }

    return cfg;
}
