#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file-system.h>

#if defined(_WIN32)
    #include <sys/stat.h>
    #include <direct.h>      /* _mkdir()   */
    #define MKDIR(path) _mkdir(path)
    #define PATH_SEP '\\'
#else
    #include <sys/stat.h>    /* mkdir()   */
    #include <sys/types.h>
    #define MKDIR(path) mkdir((path), 0755)
    #define PATH_SEP '/'
#endif

const char *get_user_home(void) {
#if defined(_WIN32)
    return getenv("USERPROFILE");
#else
    return getenv("HOME") ? : "/tmp";
#endif
}

char *config_path(const char *app_name) {
#if defined(_WIN32)
    /* Windows: %APPDATA% */
    const char *appdata = getenv("APPDATA");
    if (!appdata) {
        appdata = get_user_home();
    }
    size_t len = strlen(appdata) + 1 + strlen(app_name) + 2;
    char *p = malloc(len);
    snprintf(p, len, "%s\\%s", appdata, app_name);
    return p;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *base = xdg ? xdg : get_user_home();
    size_t len = strlen(base) + 1 + strlen(app_name) + 2; /* slash */
    char *p = malloc(len);
    snprintf(p, len, "%s/%s", base, app_name);
    return p;
#endif
}

/* ---------------  helper --------------------------------------------------- */
/* Recursively create all directories in `full_path`. Returns 0 on success,
 * or a non‑zero errno value (or -1 if we hit an unexpected error). */
int make_dirs_in_path(const char *full_path)
{
    /* Make a mutable copy because we’ll modify it. */
    char *path = strdup(full_path);
    if (!path) return ENOMEM;

    size_t len = strlen(path);

    /* If the path ends with a separator, strip it so that we don’t try to
       create an empty component. */
    if (len > 0 && path[len-1] == PATH_SEP)
        path[--len] = '\0';

    /* Walk through each component. */
    for (size_t i = 0; i < len; ++i) {
        if (path[i] != PATH_SEP) continue;

        /* Temporarily terminate the string at this separator. */
        char savec = path[i];
        path[i] = '\0';

        /* If component is empty we’re at a leading slash or doubled
           separators – skip it. */
        if (i > 0 && path[i-1] == PATH_SEP) {
            path[i] = savec;
            continue;
        }

        /* On Windows, skip the drive component ("C:") – it’s not a directory
           to create. */
#if defined(_WIN32)
        if (i == 2 && path[0] != '\0' && path[1] == ':') {
            path[i] = savec;
            continue;
        }
#endif

        /* Check existence. */
        struct stat st;
        int rc = stat(path, &st);
        if (rc != 0) {          /* does not exist or error */
            if (errno == ENOENT) {
                /* Create it. */
                if (MKDIR(path) != 0) {
                    free(path);
                    return errno;   /* e.g. EACCES, ENOSPC … */
                }
            } else {
                free(path);
                return errno;       /* stat error – propagate */
            }
        } else if (!S_ISDIR(st.st_mode)) {
            free(path);
            return ENOTDIR;          /* something exists but isn’t a dir */
        }

        /* Restore the separator and continue. */
        path[i] = savec;
    }

    /* Finally, ensure that the *full* directory exists (in case there was
       no trailing slash). */
    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (MKDIR(full_path) != 0)
            return errno;
    }

    free(path);
    return 0;   /* success */
}
