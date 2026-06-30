#ifndef AVAR_DOWNLOAD_IO_H
#define AVAR_DOWNLOAD_IO_H

#include <stdbool.h>

/* Marks the current thread as performing an active download for item_id. */
void download_io_scope_begin(const char *item_id);

/* Ends the active-download write scope on the current thread. */
void download_io_scope_end(void);

/* True when state.json at path may be written by the current caller. */
bool download_io_state_write_allowed(const char *path);

#endif
