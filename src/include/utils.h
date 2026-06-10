#ifndef AVAR_UTILS_H
#define AVAR_UTILS_H

#include <uchar.h>

#define string      const wchar_t  *
#define stringa     const char  *

bool is_valid_url(stringa url, stringa validSchemes[]);

bool is_valid_http_url(stringa url);

int print_help(int help_message_n, const char* help_message[]);

/* Duplicates at most n bytes. Caller must free(). Not available on all platforms. */
char *strndup(stringa s, size_t n);

typedef enum {
    AVAR_SIZE_BYTES,
    AVAR_SIZE_KIB,
    AVAR_SIZE_MIB,
    AVAR_SIZE_GIB,
} AvarSizeUnit;

typedef enum {
    AVAR_SPEED_BYTES_PER_SEC,
    AVAR_SPEED_BITS_PER_SEC,
    AVAR_SPEED_KIB_PER_SEC,
    AVAR_SPEED_MIB_PER_SEC,
    AVAR_SPEED_GIB_PER_SEC,
    AVAR_SPEED_KIBIT_PER_SEC,
    AVAR_SPEED_MIBIT_PER_SEC,
    AVAR_SPEED_GIBIT_PER_SEC,
} AvarSpeedUnit;

bool avar_size_unit_parse(stringa text, AvarSizeUnit *out);

bool avar_speed_unit_parse(stringa text, AvarSpeedUnit *out);

/* Formats bytes using the requested size unit. Returns buf. */
char *format_data_size(uint64_t bytes, AvarSizeUnit unit, char *buf, size_t buflen);

/* Formats a transfer rate (bytes/s) using the requested speed unit. Returns buf. */
char *format_transfer_rate(double bytes_per_sec, AvarSpeedUnit unit, char *buf, size_t buflen);

/* Builds an ASCII progress bar like "[=======              ]". Returns buf. */
char *format_progress_bar(int percent, int width, char *buf, size_t buflen);

#endif
