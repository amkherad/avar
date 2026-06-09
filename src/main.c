#include <cli.h>

#include "logger.h"
#include "mongoose.h"

static int loglevel_to_mg_ll(log_level_t logLevel);
static void mg_log_to_log(char c, void *param);


int main(int argc, char *argv[]) {
    mg_log_set(loglevel_to_mg_ll(get_log_level()));

    return cli_run(argc, argv);
}

int loglevel_to_mg_ll(log_level_t logLevel) {
    switch (logLevel) {
        case LOG_LEVEL_DEBUG: return MG_LL_DEBUG;
        case LOG_LEVEL_INFO:
        case LOG_LEVEL_WARNING: return MG_LL_INFO;
        case LOG_LEVEL_ERROR:
        case LOG_LEVEL_FATAL: return MG_LL_ERROR;
    }
}

void mg_log_to_log(char c, void *param) {
    // LOG_
}

static int mg_log_level_ticket = MG_LL_INFO;

void mg_log_prefix(int level, const char *file, int line, const char *fname) {
    mg_log_level_ticket = level;

    // const char *p = strrchr(file, '/');
    // char buf[41];
    // size_t n;
    // if (p == NULL) p = strrchr(file, '\\');
    // n = mg_snprintf(buf, sizeof(buf), "%-6llx %d %s:%d:%s", mg_millis(), level,
    //                 p == NULL ? file : p + 1, line, fname);
    // if (n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    // while (n < sizeof(buf)) buf[n++] = ' ';
    // logs(buf, n - 1);
}

void mg_log(const char *fmt, ...) {
    log_level_t log_level;
    switch (mg_log_level_ticket) {
        case MG_LL_INFO:
            log_level = LOG_LEVEL_INFO;
            break;
        case MG_LL_ERROR:
            log_level = LOG_LEVEL_ERROR;
            break;
        case MG_LL_DEBUG:
        case MG_LL_VERBOSE:
        default:
            log_level = LOG_LEVEL_DEBUG;
    }

    va_list ap;
    va_start(ap, fmt);
    VLOG(log_level, fmt, ap);
    va_end(ap);
}
