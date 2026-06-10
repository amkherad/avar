#ifndef AVAR_DOWNLOAD_STATE_H
#define AVAR_DOWNLOAD_STATE_H

#include "avar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *id;
    char *url;
    char *filename;
    char *temp_path;
    char *dest_path;
    char *status;
    char *proxy;
    char *queued_at;
    char *last_try_at;
    char *description;
    char *original_page;
    char *referer;
    char *added_through;
    char *queue_id;
    char *etag;
    char *last_modified;
    uint64_t total_size;
    uint64_t bytes_downloaded;
    size_t chunk_size;
    size_t chunk_count;
    bool *chunks_done;
} DownloadState;

void download_state_free(DownloadState *state);

/* Loads state from path. Returns NULL if missing or invalid. */
DownloadState *download_state_load(const char *path);

/* Persists state to path. Returns 0 on success, -1 on failure. */
int download_state_save(const DownloadState *state, const char *path);

/* Allocates a fresh state for a new download. Caller must free with download_state_free(). */
DownloadState *download_state_create(const char *url, const char *filename,
                                     const char *temp_path, const char *dest_path,
                                     uint64_t total_size, size_t chunk_size);

size_t download_state_completed_chunks(const DownloadState *state);

uint64_t download_state_bytes_done(const DownloadState *state);

bool download_state_all_chunks_done(const DownloadState *state);

#endif
