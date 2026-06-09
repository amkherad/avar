#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Foreground colors */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"

/* Background colors */
#define BKG_RED     "\x1b[41m"
#define BKG_GREEN   "\x1b[42m"
/* … */

/* Reset everything back to defaults */
#define COLOR_RESET  "\x1b[0m"

/* ------------------------------------------------------------------ */
/*  Generic color type – you can change the enum as needed            */
typedef enum {
    CONSOLE_COLOR_NORMAL = 0,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_WHITE
} ConsoleColor;

/* --- Severity levels ---------------------------------------------- */
typedef enum {
    LOG_LEVEL_TRACE = -1,
    LOG_LEVEL_VERBOSE = 0,
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;

typedef struct {
    FILE *out; /* where to write (stdout, stderr, file) */
    log_level_t min_lvl; /* messages below this are dropped */
} logger_t;

void set_log_level(log_level_t lvl);

log_level_t get_log_level();

bool is_log_level(log_level_t lvl);

void init_logger(bool isVerbose);

/* Forward declaration for static inline helpers */

void _vlog_msg(log_level_t lvl, const char *file,
               const char *func, int line, const char *fmt, va_list va_list);

void _log_msg(log_level_t lvl, const char *file,
              const char *func, int line, const char *fmt, ...);

/* ------------------------------------------------------------------- */
/* Macros that automatically inject file/line/function information    */
#if defined(__GNUC__) || defined(__clang__)
#   define LOG_FUNC __func__
#else
#   define LOG_FUNC ""
#endif

#define LOG(level, fmt, ...) \
    _log_msg((level), __FILE__, LOG_FUNC, __LINE__, (fmt), ##__VA_ARGS__)

#define VLOG(level, fmt, va_list) \
    _vlog_msg((level), __FILE__, LOG_FUNC, __LINE__, (fmt), va_list)

#define LOG_DUMP(fmt, ...)  LOG(LOG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)
#define LOG_VERBOSE(fmt, ...)  LOG(LOG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  LOG(LOG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   LOG(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  LOG(LOG_LEVEL_ERROR,  fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)  LOG(LOG_LEVEL_FATAL,  fmt, ##__VA_ARGS__)
//
// // size_t
// #define LOGI(value) \
//     printf("[%s():%d] " #value " == %i \n", __func__, __LINE__, value)
//
// // size_t
// #define LOGD(value) \
//     printf("[%s():%d] " #value " == %d \n", __func__, __LINE__, value)
//
// // boolean
// #define LOGB(value) \
//     printf("[%s():%d] (" #value ") == %s \n", __func__, __LINE__, (value ? "true" : "false"))
//
// // unsigned
// #define LOGU(value) \
//     printf("[%s():%d] " #value " == %u \n", __func__, __LINE__, value)
//
// // double, fixed-point notation
// #define LOGF(value) \
//     printf("[%s():%d] " #value " == %f \n", __func__, __LINE__, value)
//
// // double, standard form
// #define LOGE(value) \
//     printf("[%s():%d] " #value " == %e \n", __func__, __LINE__, value)
//
// // double, normal or exponential notation, whichever is more appropriate
// #define LOGG(value) \
//     printf("[%s():%d] " #value " == %g \n", __func__, __LINE__, value)
//
// // unsigned, hexadecimal
// #define LOGX(value) \
//     printf("[%s():%d] " #value " == %x \n", __func__, __LINE__, value)
//
// // unsigned, octal
// #define LOGO(value) \
//     printf("[%s():%d] " #value " == %o \n", __func__, __LINE__, value)
//
// // string
// #define LOGS(value) \
//     printf("[%s():%d] " #value " == \"%s\" \n", __func__, __LINE__, value)
//
// // char
// #define LOGC(value) \
//     printf("[%s():%d] " #value " == \'%c\' \n", __func__, __LINE__, value)
//
// // pointer
// #define LOGP(value) \
//     printf("[%s():%d] " #value " == %p \n", __func__, __LINE__, value)

#endif // guard
