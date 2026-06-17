#ifndef AVAR_GUI_EMBED_H
#define AVAR_GUI_EMBED_H

#include <stdbool.h>
#include <stddef.h>

struct mg_connection;

#if defined(AVAR_EMBED_GUI)

bool gui_embed_available(void);

/* Serves an embedded static asset when uri maps to a bundled file. Returns true when handled. */
bool gui_embed_serve(struct mg_connection *connection, const char *uri);

#else

static inline bool gui_embed_available(void) {
    return false;
}

static inline bool gui_embed_serve(struct mg_connection *connection, const char *uri) {
    (void)connection;
    (void)uri;
    return false;
}

#endif

#endif
