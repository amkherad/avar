#ifndef AVAR_BOOKMARK_H
#define AVAR_BOOKMARK_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    BookmarkErrorNone = 0,
    BookmarkErrorNotFound,
    BookmarkErrorInvalidArg,
    BookmarkErrorPersist,
} BookmarkError;

/**
 * Adds or updates a bookmark for url. On success, writes a newly allocated id to
 * id_out when the bookmark was created (NULL on update). Caller must free id_out.
 */
BookmarkError bookmark_add(const char *url, const char *title, uint32_t link_count,
                           char **id_out);

/** Removes a bookmark by id or url (when by_url is true). */
BookmarkError bookmark_remove(const char *id_or_url, bool by_url);

/** Returns true when url is bookmarked. */
bool bookmark_has_url(const char *url);

size_t bookmark_count(void);

/**
 * Returns a newly allocated JSON array of bookmark objects, or NULL on failure.
 * Caller must free().
 */
char *bookmark_list_json(void);

#endif
