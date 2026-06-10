#include <avar.h>
#include <config.h>
#include <file-system.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <direct.h>      /* _mkdir()   */
    #include <sys/stat.h>
    #include <windows.h>
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

char *path_join(const char *dir, const char *name) {
    if (dir == NULL || name == NULL) {
        return NULL;
    }

    const size_t dir_len = strlen(dir);
    const size_t name_len = strlen(name);
    const bool needs_sep = dir_len > 0 && dir[dir_len - 1] != PATH_SEP && dir[dir_len - 1] != '/'
                           && dir[dir_len - 1] != '\\';

    const size_t total = dir_len + (needs_sep ? 1 : 0) + name_len + 1;
    char *out = malloc(total);
    if (out == NULL) {
        return NULL;
    }

    if (needs_sep) {
        snprintf(out, total, "%s%c%s", dir, PATH_SEPARATOR, name);
    } else {
        snprintf(out, total, "%s%s", dir, name);
    }

    return out;
}

static char *platform_data_dir(void) {
#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (appdata == NULL) {
        appdata = get_user_home();
    }
    return path_join(appdata, APP_ID);
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg != NULL) {
        return path_join(xdg, APP_ID);
    }
    char *home = path_join(get_user_home(), ".local/share");
    if (home == NULL) {
        return NULL;
    }
    char *result = path_join(home, APP_ID);
    free(home);
    return result;
#endif
}

char *default_temp_path(void) {
    char *configured = get_config("dm.tempPath");
    if (configured != NULL) {
        return configured;
    }

    char *base = platform_data_dir();
    if (base == NULL) {
        return NULL;
    }

    char *temp = path_join(base, "download-temp");
    free(base);
    if (temp != NULL) {
        (void)make_dirs_in_path(temp);
    }
    return temp;
}

char *default_download_path(void) {
    char *configured = get_config("dm.downloadPath");
    if (configured != NULL) {
        return configured;
    }

#if defined(_WIN32)
    const char *profile = getenv("USERPROFILE");
    if (profile == NULL) {
        profile = get_user_home();
    }
    return path_join(profile, "Downloads");
#elif defined(__APPLE__)
    char *home = path_join(get_user_home(), "Downloads");
    if (home != NULL) {
        (void)make_dirs_in_path(home);
    }
    return home;
#else
    const char *xdg_download = getenv("XDG_DOWNLOAD_DIR");
    if (xdg_download != NULL) {
        char *path = strdup(xdg_download);
        if (path != NULL) {
            (void)make_dirs_in_path(path);
        }
        return path;
    }

    char *home = path_join(get_user_home(), "Downloads");
    if (home != NULL) {
        (void)make_dirs_in_path(home);
    }
    return home;
#endif
}

char *sanitize_filename(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    const size_t len = strlen(name);
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        const unsigned char c = (unsigned char)name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<'
            || c == '>' || c == '|' || c < 32) {
            continue;
        }
        out[j++] = (char)c;
    }

    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '.')) {
        j--;
    }

    if (j == 0) {
        free(out);
        return NULL;
    }

    out[j] = '\0';
    return out;
}

bool file_exists(const char *path) {
    if (path == NULL) {
        return false;
    }

    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int move_file_atomic(const char *src, const char *dest) {
    if (src == NULL || dest == NULL) {
        return -1;
    }

#if defined(_WIN32)
    if (MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return -1;
    }
    return 0;
#else
    if (rename(src, dest) != 0) {
        if (errno != EXDEV) {
            return -1;
        }

        FILE *in = fopen(src, "rb");
        if (in == NULL) {
            return -1;
        }

        FILE *out = fopen(dest, "wb");
        if (out == NULL) {
            fclose(in);
            return -1;
        }

        char buffer[64 * 1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof buffer, in)) > 0) {
            if (fwrite(buffer, 1, n, out) != n) {
                fclose(in);
                fclose(out);
                remove(dest);
                return -1;
            }
        }

        if (ferror(in)) {
            fclose(in);
            fclose(out);
            remove(dest);
            return -1;
        }

        fclose(in);
        if (fflush(out) != 0) {
            fclose(out);
            remove(dest);
            return -1;
        }
        fclose(out);
        remove(src);
        return 0;
    }
    return 0;
#endif
}
