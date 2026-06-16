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
int transient_download(const char *url, const char *queue, const char *name, bool attached);

/** Starts a background download in the daemon. Writes optional id to id_out. */
int download_start_background(const char *url, const char *queue, const char *name, char **id_out);

size_t download_active_count(void);

/**
 * Fills items with downloads currently in the downloading state.
 * Returns the number written (may exceed max_items; caller should size the buffer).
 */
size_t download_active_list(DownloadActiveInfo *items, size_t max_items);

/** Waits until no active downloads or timeout. Returns true when idle. */
bool download_wait_idle(unsigned timeout_ms);

#endif
