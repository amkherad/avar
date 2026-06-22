#ifndef AVAR_FILE_SYSTEM_H
#define AVAR_FILE_SYSTEM_H

#include "avar.h"

#include <stdbool.h>

const char *get_user_home(void);

char *config_path(const char *app_name);

int make_dirs_in_path(const char *full_path);

/* Joins dir and name with PATH_SEPARATOR. Caller must free(). */
char *path_join(const char *dir, const char *name);

/* Returns dm.tempPath or the platform default. Caller must free(). */
char *default_temp_path(void);

/* Returns dm.downloadPath or the platform default. Caller must free(). */
char *default_download_path(void);

/* Sanitizes a filename for the current platform. Caller must free(). */
char *sanitize_filename(const char *name);

/* Atomically moves src to dest (replace if dest exists). Returns 0 on success. */
int move_file_atomic(const char *src, const char *dest);

/* Returns dest_path, or a variant with " (n)" before the extension when dest exists. Caller must free(). */
char *resolve_unique_dest_path(const char *dest_path);

/* Returns true when path exists and is a regular file. */
bool file_exists(const char *path);

/* Recursively deletes a directory and its contents. Returns 0 on success. */
int remove_directory_recursive(const char *path);

#endif
