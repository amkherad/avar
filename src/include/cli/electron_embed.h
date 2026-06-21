#ifndef AVAR_ELECTRON_EMBED_H
#define AVAR_ELECTRON_EMBED_H

#include <stdbool.h>
#include <stddef.h>

#if defined(AVAR_EMBED_ELECTRON)

bool electron_embed_available(void);

/** Extract the embedded Electron bundle when needed and write the executable path. */
bool electron_embed_resolve_exe(char *exe_path, size_t exe_path_len);

#else

static inline bool electron_embed_available(void) {
    return false;
}

static inline bool electron_embed_resolve_exe(char *exe_path, size_t exe_path_len) {
    (void)exe_path;
    (void)exe_path_len;
    return false;
}

#endif

#endif
