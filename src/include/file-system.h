#ifndef AVAR_FILE_SYSTEM_H
#define AVAR_FILE_SYSTEM_H

#if defined(_WIN32)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

const char *get_user_home(void);

char *config_path(const char *app_name);

int make_dirs_in_path(const char *full_path);

#endif
