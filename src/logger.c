#include <avar.h>
#include <config.h>
#include <file-system.h>
#include <logger.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
static int ascii_stricmp(const char *a, const char *b) {
    return _stricmp(a, b);
}
#else
#include <strings.h>
static int ascii_stricmp(const char *a, const char *b) {
    return strcasecmp(a, b);
}
#endif

static logger_t g_logger = {
    .out = NULL,
    .min_lvl = LOG_LEVEL_INFO,
};
static FILE *g_log_file = NULL;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void daemon_rpc_log_append(const char *line) {
    (void)line;
}
#endif

#ifdef _WIN32
#include <windows.h>
static HANDLE g_hStdOut = NULL;
#endif

static void init_console(void);
static void set_console_color(ConsoleColor color);
static void reset_console_color(void);
static void append_log_line(const char *line);
static void open_log_file_from_config(void);

static bool parse_config_bool(const char *key, const bool default_value) {
    char *value = get_config_or_default(key, NULL);
    if (value == NULL) {
        return default_value;
    }

    const bool result =
            strcmp(value, "1") == 0 || ascii_stricmp(value, "true") == 0 || ascii_stricmp(value, "yes") == 0;
    free(value);
    return result;
}

void init_logger(bool isVerbose) {
    init_console();
    g_logger.out = stdout;
    g_logger.min_lvl = isVerbose ? LOG_LEVEL_VERBOSE : LOG_LEVEL_INFO;
    logger_apply_config();
}

void logger_apply_config(void) {
    logger_close();

    if (!parse_config_bool(AVAR_CFG_LOG_FILE_ENABLED, false)) {
        return;
    }

    char *path = get_config_or_default(AVAR_CFG_LOG_FILE_PATH, NULL);
    if (path == NULL || path[0] == '\0') {
        free(path);
        return;
    }

    char *parent_sep = strrchr(path, PATH_SEPARATOR);
    if (parent_sep != NULL) {
        char *parent = strndup(path, (size_t)(parent_sep - path));
        if (parent != NULL) {
            (void)make_dirs_in_path(parent);
            free(parent);
        }
    }

    g_log_file = fopen(path, "a");
    free(path);
}

void logger_close(void) {
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static void init_console(void) {
#ifdef _WIN32
    if (!g_hStdOut) {
        g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (g_hStdOut == INVALID_HANDLE_VALUE) {
            g_hStdOut = NULL;
        }

#ifdef NTDDI_WIN10
        DWORD mode;
        if (GetConsoleMode(g_hStdOut, &mode)) {
            SetConsoleMode(g_hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
#endif
}

void set_logger(logger_t lvl) { g_logger = lvl; }

log_level_t get_log_level() { return g_logger.min_lvl; }

bool is_log_level(log_level_t lvl) { return lvl <= g_logger.min_lvl; }

static void append_log_line(const char *line) {
    if (line == NULL) {
        return;
    }

    daemon_rpc_log_append(line);

    if (g_log_file != NULL) {
        fputs(line, g_log_file);
        fputc('\n', g_log_file);
        fflush(g_log_file);
    }
}

void _vlog_msg(log_level_t lvl, const char *file, const char *func, int line, const char *fmt,
               va_list va_list) {
    (void)file;
    (void)func;
    (void)line;

    if (lvl < g_logger.min_lvl) {
        return;
    }

    static const char *level_str[] = {"DBG", "INF", "WAR", "ERR", "FTL"};
    static const ConsoleColor color_mapping[] = {
        CONSOLE_COLOR_NORMAL, CONSOLE_COLOR_WHITE, CONSOLE_COLOR_YELLOW,
        CONSOLE_COLOR_RED,    CONSOLE_COLOR_RED,
    };

    time_t t = time(NULL);
    struct tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    const ConsoleColor color = color_mapping[lvl];
    if (color != CONSOLE_COLOR_NORMAL) {
        set_console_color(color);
    }

    char prefix[64];
    snprintf(prefix, sizeof prefix, "%04d-%02d-%02d-%02d:%02d:%02d [%s]: ", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, level_str[lvl]);

    char message[512];
    vsnprintf(message, sizeof message, fmt, va_list);

    FILE *out = g_logger.out != NULL ? g_logger.out : stdout;
    fprintf(out, "%s%s\n", prefix, message);

    if (color != CONSOLE_COLOR_NORMAL) {
        reset_console_color();
    }

    if (lvl == LOG_LEVEL_VERBOSE || lvl == LOG_LEVEL_FATAL) {
        fflush(out);
    }

    char full_line[576];
    snprintf(full_line, sizeof full_line, "%s%s", prefix, message);
    append_log_line(full_line);
}

void _log_msg(log_level_t lvl, const char *file, const char *func, int line, const char *fmt,
              ...) {
    if (lvl < g_logger.min_lvl) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    _vlog_msg(lvl, file, func, line, fmt, ap);
    va_end(ap);
}

inline void fatal(const char *reason) {
    if (reason) {
        LOG_FATAL("Fatal error: %s", reason);
    }
    exit(EXIT_FATAL_ERROR);
}

static void set_console_color(ConsoleColor color) {
#if defined(_WIN32) && !defined(NTDDI_WIN10)
    static HANDLE hOut = NULL;
    if (!hOut) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    WORD attributes = 0x07;
    switch (color) {
        case CONSOLE_COLOR_RED: attributes |= FOREGROUND_RED; break;
        case CONSOLE_COLOR_GREEN: attributes |= FOREGROUND_GREEN; break;
        case CONSOLE_COLOR_YELLOW: attributes |= FOREGROUND_RED | FOREGROUND_GREEN; break;
        case CONSOLE_COLOR_BLUE: attributes |= FOREGROUND_BLUE; break;
        case CONSOLE_COLOR_MAGENTA: attributes |= FOREGROUND_RED | FOREGROUND_BLUE; break;
        case CONSOLE_COLOR_CYAN: attributes |= FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case CONSOLE_COLOR_WHITE:
            attributes |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
        default: attributes = 0x07;
    }

    if (!SetConsoleTextAttribute(hOut, attributes)) {
        printf("Unable to set console text attributes");
    }
#else
    switch (color) {
        case CONSOLE_COLOR_RED: printf("\x1b[31m"); break;
        case CONSOLE_COLOR_GREEN: printf("\x1b[32m"); break;
        case CONSOLE_COLOR_YELLOW: printf("\x1b[33m"); break;
        case CONSOLE_COLOR_BLUE: printf("\x1b[34m"); break;
        case CONSOLE_COLOR_MAGENTA: printf("\x1b[35m"); break;
        case CONSOLE_COLOR_CYAN: printf("\x1b[36m"); break;
        case CONSOLE_COLOR_WHITE: printf("\x1b[37m"); break;
        default: printf("\x1b[0m");
    }
#endif
}

static void reset_console_color(void) {
#if defined(_WIN32) && !defined(NTDDI_WIN10)
    set_console_color(CONSOLE_COLOR_NORMAL);
#else
    printf("\x1b[0m");
#endif
}
