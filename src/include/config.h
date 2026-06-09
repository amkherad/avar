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

/* Array helpers (e.g. dm.items). */

size_t get_config_array_size(stringa key);

/* Returns a newly allocated string for a field inside an array object, or NULL. */
char *get_config_array_item_field(stringa key, size_t index, stringa field);

/* Appends a JSON object string to an array. Returns 0 on success, -1 on failure. */
int append_config_array_item(stringa key, stringa json_object);

/* Updates the first array item where match_field == match_value. */
int update_config_array_item(stringa key, stringa match_field, stringa match_value,
                             stringa json_object);

/* Removes the first array item where match_field == match_value. */
int remove_config_array_item(stringa key, stringa match_field, stringa match_value);

#if defined(AVAR_TESTING)
/* Opens an isolated config file for unit tests. */
int config_open_at(stringa config_file);
#endif

#endif
