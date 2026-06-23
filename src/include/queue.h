#ifndef AVAR_QUEUE_H
#define AVAR_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    QueueErrorNone = 0,
    QueueErrorNotFound,
    QueueErrorDuplicateName,
    QueueErrorInvalidArg,
    QueueErrorPersist,
} QueueError;

typedef struct QueueOptions {
    const char *description;
    uint32_t max_concurrent_downloads;
    uint32_t max_connections;
    uint32_t max_retries;
    const char *temp_path;
    const char *download_path;
} QueueOptions;

typedef struct QueuePatch {
    bool set_description;
    const char *description;

    bool set_max_concurrent_downloads;
    uint32_t max_concurrent_downloads;

    bool set_max_connections;
    uint32_t max_connections;

    bool set_max_retries;
    uint32_t max_retries;

    bool set_temp_path;
    const char *temp_path;

    bool set_download_path;
    const char *download_path;
} QueuePatch;

/**
 * Creates a queue with the given name and optional settings.
 * On success, writes a newly allocated id to id_out (caller must free()).
 */
QueueError queue_add(const char *name, const QueueOptions *options, char **id_out);

/**
 * Removes a queue by id or name. When purge_items is false, download items are
 * detached (queueId cleared). When true, matching download items are removed.
 */
QueueError queue_remove(const char *id_or_name, bool by_name, bool purge_items);

/** Applies partial updates to the queue identified by id. */
QueueError queue_edit(const char *id, const QueuePatch *patch);

/** Schedules the queue (stub: logs until scheduler integration). */
QueueError queue_start(const char *id);

/** Stops active downloads in the queue by resetting their persisted status. */
QueueError queue_stop(const char *id);

/** Returns true when the queue scheduler flag is set. */
bool queue_is_started(const char *id);

size_t queue_count(void);

/**
 * Returns a newly allocated id for the queue at index, or NULL. Caller must free().
 */
char *queue_id_at(size_t index);

/**
 * Returns a newly allocated name for the queue at index, or NULL. Caller must free().
 */
char *queue_name_at(size_t index);

/**
 * Resolves a queue id from an id or name string. Returns a newly allocated id,
 * or NULL when not found. Caller must free().
 */
char *queue_resolve_id(const char *id_or_name, bool by_name);

#endif
