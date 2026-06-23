#ifndef AVAR_DOWNLOAD_H
#define AVAR_DOWNLOAD_H

#include <avar.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define AVAR_DL_FILENAME_BUF_SIZE 260U

typedef struct {
    char id[AVAR_DL_ID_BUF_SIZE];
    char filename[AVAR_DL_FILENAME_BUF_SIZE];
    uint64_t bytes_downloaded;
    uint64_t total_bytes;
} DownloadActiveInfo;

/* Runs an attached (foreground) HTTP(S) download with progress output. */
int transient_download(const char *url, const char *queue, const char *name, const char *proxy_url,
                       bool attached);

/** Starts a background download in the daemon. Writes optional id to id_out. */
int download_start_background(const char *url, const char *queue, const char *name, char **id_out);

int download_start_background_with_proxy(const char *url, const char *queue, const char *name,
                                         const char *proxy_url, char **id_out);

/** Adds a download to dm.items with queued status without starting it. */
int download_enqueue_with_proxy(const char *url, const char *queue, const char *name,
                                const char *proxy_url, char **id_out);

int download_enqueue_ex(const char *url, const char *queue, const char *name,
                        const char *proxy_url, const char *stream_kind, const char *referer,
                        char **id_out);

size_t download_active_count(void);

/**
 * Fills items with downloads currently in the downloading state.
 * Returns the number written (may exceed max_items; caller should size the buffer).
 */
size_t download_active_list(DownloadActiveInfo *items, size_t max_items);

/** Registers a GUI client watching segmented progress for id (reference counted). */
void download_progress_watch(const char *id);

/** Unregisters a segmented-progress watcher for id. */
void download_progress_unwatch(const char *id);

/** Returns true when at least one client is watching segmented progress for id. */
bool download_progress_is_watched(const char *id);

/** Waits until no active downloads or timeout. Returns true when idle. */
bool download_wait_idle(unsigned timeout_ms);

/**
 * Removes a download item from dm.items by id or filename.
 * When purge_files is true, deletes on-disk partial and completed files.
 */
int download_remove(const char *target, bool by_id, bool purge_files, bool force);

int download_pause(const char *id);
int download_resume(const char *id);
int download_start(const char *id);
int download_stop(const char *id);

/** Resumes downloads left in the downloading state and restarts started queues. */
void download_resume_interrupted(void);

/**
 * Resolves the on-disk path of a completed download item.
 * Writes a newly allocated path to path_out on success. Caller must free(*path_out).
 */
int download_resolve_dest_path(const char *id, char **path_out);

#if defined(AVAR_TESTING)
char *download_test_choose_filename(const char *url, const char *header_value, size_t header_len);

bool download_test_parse_content_range_total(const char *header, uint64_t *total_out);

uint64_t download_test_existing_file_size(const char *path);

char *download_test_generate_id(void);
#endif

#endif
