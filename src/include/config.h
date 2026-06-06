#ifndef AVAR_CONFIG_H
#define AVAR_CONFIG_H

#include "utils.h"

/* Dot-separated keys, e.g. "download.limiter.speed". */

/* Returns a newly allocated string, or NULL if missing. Caller must free(). */
char *get_config(stringa key);

/* Returns a newly allocated string, or NULL if missing and no default. Caller must free(). */
char *get_config_or_default(stringa key, stringa default_value);

/* Returns 0 on success, -1 on failure. Persists to the default config file. */
int set_config(stringa key, stringa value);

/* Removes a key (and empty parent objects). Returns 0 on success, -1 if missing. */
int reset_config(stringa key);

/* Clears all configuration. Returns 0 on success, -1 on failure. */
int reset_all_config(void);

/* Writes the in-memory config to path. Returns 0 on success, -1 on failure. */
int save_config(stringa path);

/* Replaces the in-memory config from path. Returns 0 on success, -1 on failure. */
int load_config(stringa path);

#endif
