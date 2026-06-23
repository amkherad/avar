#ifndef AVAR_UTILS_H
#define AVAR_UTILS_H

#include <stdint.h>
#include <wchar.h>

#define string      const wchar_t  *
#define stringa     const char  *

bool is_valid_url(stringa url, stringa validSchemes[]);

bool is_valid_http_url(stringa url);

void trim_whitespace_inplace(char *value);

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
    AVAR_PROGRESS_AGGREGATE,
    AVAR_PROGRESS_SEGMENTED,
} AvarProgressStyle;

typedef struct {
    uint64_t start;
    uint64_t end; /* inclusive */
} AvarByteRange;

typedef bool (*AvarColumnFilledFn)(uint64_t col_start, uint64_t col_end, void *ctx);

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

bool avar_progress_style_parse(stringa text, AvarProgressStyle *out);

/* True when stderr is an interactive terminal (progress may use full width). */
bool avar_stderr_is_tty(void);

/* Terminal width in columns, or 0 when unknown. */
int avar_terminal_columns(void);

/* Inner bar width so "[bar]" plus suffix fits within cols (or fixed width when cols <= 0). */
int avar_progress_bar_width_for_columns(int cols, int suffix_len);

/* Bar width for the suffix already formatted after the bar (leading space included). */
int avar_progress_bar_width(int suffix_len);

/* Formats bytes using the requested size unit. Returns buf. */
char *format_data_size(uint64_t bytes, AvarSizeUnit unit, char *buf, size_t buflen);

/* Right-aligns the percent in DL_PROGRESS_PERCENT_WIDTH digits (e.g. "  7%"). Returns buf. */
char *format_progress_percent(int percent, char *buf, size_t buflen);

/* Returns the character width of the numeric part for bytes in unit (e.g. 192 -> 3). */
int avar_data_size_number_width(uint64_t bytes, AvarSizeUnit unit);

/* Like format_data_size(), but right-aligns the numeric part in number_width columns. */
char *format_data_size_padded(uint64_t bytes, AvarSizeUnit unit, int number_width, char *buf,
                              size_t buflen);

/* Formats a transfer rate (bytes/s) using the requested speed unit. Returns buf. */
char *format_transfer_rate(double bytes_per_sec, AvarSpeedUnit unit, char *buf, size_t buflen);

/* Like format_transfer_rate(), but right-aligns the numeric part in number_width columns. */
char *format_transfer_rate_padded(double bytes_per_sec, AvarSpeedUnit unit, int number_width,
                                  char *buf, size_t buflen);

/* Builds an ASCII progress bar like "[=======              ]". Returns buf. */
char *format_progress_bar(int percent, int width, char *buf, size_t buflen);

/* Maps downloaded byte ranges onto a single spatial bar. Returns buf. */
char *format_spatial_progress_bar(uint64_t total_size, const AvarByteRange *ranges,
                                  size_t range_count, int width, char *buf, size_t buflen);

/* Like format_spatial_progress_bar(), but asks is_filled() per column slice. Returns buf. */
char *format_spatial_progress_bar_fn(uint64_t total_size, AvarColumnFilledFn is_filled, void *ctx,
                                     int width, char *buf, size_t buflen);

#endif
