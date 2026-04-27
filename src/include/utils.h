#ifndef AVAR_UTILS_H
#define AVAR_UTILS_H

#include <uchar.h>

#define string      const wchar_t  *
#define stringa     const char  *

bool is_valid_url(stringa url, stringa validSchemes[]);

bool is_valid_http_url(stringa url);

int print_help(int help_message_n, const char* help_message[]);

#endif
