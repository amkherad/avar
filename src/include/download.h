#ifndef AVAR_DOWNLOAD_H
#define AVAR_DOWNLOAD_H

#include <stdbool.h>
#include <stdlib.h>

/* Runs an attached (foreground) HTTP(S) download with progress output. */
int transient_download(const char *url, const char *queue, const char *name, bool attached);

/** Starts a background download in the daemon. Writes optional id to id_out. */
int download_start_background(const char *url, const char *queue, const char *name, char **id_out);

size_t download_active_count(void);

/** Waits until no active downloads or timeout. Returns true when idle. */
bool download_wait_idle(unsigned timeout_ms);

#endif
