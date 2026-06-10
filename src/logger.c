#include <avar.h>
#include <logger.h>

static logger_t g_logger;

#ifdef _WIN32
#include <windows.h>
static HANDLE g_hStdOut = NULL; /* global, initialized once */
#endif

static void init_console(void);

static void set_console_color(ConsoleColor color);

static void reset_console_color(void);

void init_logger(bool isVerbose) {
    init_console();
    g_logger.out = stdout;
    g_logger.min_lvl = isVerbose ? LOG_LEVEL_VERBOSE : LOG_LEVEL_INFO;
}

static void init_console(void) {
#ifdef _WIN32
    if (!g_hStdOut) {
        g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (g_hStdOut == INVALID_HANDLE_VALUE) {
            /* fallback: try to get a console, but don't create one */
            g_hStdOut = NULL;
        }

#ifdef NTDDI_WIN10
        /* Enable ANSI on Windows 10+ (optional – keeps the helper short) */
        DWORD mode;
        if (GetConsoleMode(g_hStdOut, &mode)) {
            SetConsoleMode(g_hStdOut,
                           mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
#endif
}

/* ------------------------------------------------------------------- */
void set_logger(logger_t lvl) { g_logger = lvl; }

log_level_t get_log_level() { return g_logger.min_lvl; }

bool is_log_level(log_level_t lvl) {
    return lvl <= g_logger.min_lvl;
}

void _vlog_msg(log_level_t lvl, const char *file,
               const char *func, int line, const char *fmt, va_list va_list) {
    if (lvl < g_logger.min_lvl) return;

    /* Simple level string */
    static const char *level_str[] = {
        "DBG", "INF", "WAR", "ERR", "FTL"
    };

    static const ConsoleColor color_mapping[] = {
        CONSOLE_COLOR_NORMAL,
        CONSOLE_COLOR_WHITE,
        CONSOLE_COLOR_YELLOW,
        CONSOLE_COLOR_RED,
        CONSOLE_COLOR_RED,
    };

    /* Timestamp */
    time_t t = time(NULL);
    struct tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    auto color = color_mapping[lvl];
    if (color != CONSOLE_COLOR_NORMAL) {
        set_console_color(color);
    }

    fprintf(g_logger.out,
            "%04d-%02d-%02d-%02d:%02d:%02d "
            "[%s]: ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            level_str[lvl]/*, func, line*/);

    vfprintf(g_logger.out, fmt, va_list);

    fprintf(g_logger.out, "\n");

    if (color != CONSOLE_COLOR_NORMAL) {
        reset_console_color();
    }

    if (lvl == LOG_LEVEL_VERBOSE || lvl == LOG_LEVEL_FATAL) {
        fflush(g_logger.out);
    }
}

void _log_msg(log_level_t lvl,
              const char *file, const char *func, int line,
              const char *fmt, ...) {
    if (lvl < g_logger.min_lvl) return;

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

/* ------------------------------------------------------------------ */
static void set_console_color(ConsoleColor color) {
#if defined(_WIN32) && !defined(NTDDI_WIN10)
    static HANDLE hOut = NULL;
    if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    /* 0x07 is the default FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
       with intensity. We keep the background unchanged. */
    WORD attributes = 0x07;

    switch (color) {
        case CONSOLE_COLOR_RED: attributes |= FOREGROUND_RED;
            break;
        case CONSOLE_COLOR_GREEN: attributes |= FOREGROUND_GREEN;
            break;
        case CONSOLE_COLOR_YELLOW: attributes |= FOREGROUND_RED | FOREGROUND_GREEN;
            break;
        case CONSOLE_COLOR_BLUE: attributes |= FOREGROUND_BLUE;
            break;
        case CONSOLE_COLOR_MAGENTA: attributes |= FOREGROUND_RED | FOREGROUND_BLUE;
            break;
        case CONSOLE_COLOR_CYAN: attributes |= FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
        case CONSOLE_COLOR_WHITE: attributes |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
        default: attributes = 0x07; /* reset */
    }

    if (!SetConsoleTextAttribute(hOut, attributes)) {
        printf("Unable to set console text attributes");
    }
#else
    /* On POSIX terminals we just print an ANSI escape code. */
    switch (color) {
        case CONSOLE_COLOR_RED: printf("\x1b[31m");
            break;
        case CONSOLE_COLOR_GREEN: printf("\x1b[32m");
            break;
        case CONSOLE_COLOR_YELLOW: printf("\x1b[33m");
            break;
        case CONSOLE_COLOR_BLUE: printf("\x1b[34m");
            break;
        case CONSOLE_COLOR_MAGENTA: printf("\x1b[35m");
            break;
        case CONSOLE_COLOR_CYAN: printf("\x1b[36m");
            break;
        case CONSOLE_COLOR_WHITE: printf("\x1b[37m");
            break;
        default: printf("\x1b[0m"); /* reset */
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
