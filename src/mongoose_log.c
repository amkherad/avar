#include "logger.h"
#include "mongoose.h"

#if MG_TLS == MG_TLS_MBED
#include <mbedtls/debug.h>

static void apply_mbedtls_debug_threshold(void) {
    if (get_log_level() <= LOG_LEVEL_DEBUG) {
        mbedtls_debug_set_threshold(2);
    } else {
        mbedtls_debug_set_threshold(0);
    }
}
#endif

static int loglevel_to_mg_ll(log_level_t log_level) {
    switch (log_level) {
        case LOG_LEVEL_DEBUG:
            return MG_LL_DEBUG;
        case LOG_LEVEL_INFO:
        case LOG_LEVEL_WARNING:
            return MG_LL_INFO;
        case LOG_LEVEL_ERROR:
        case LOG_LEVEL_FATAL:
            return MG_LL_ERROR;
        default:
            return MG_LL_INFO;
    }
}

void avar_mongoose_log_init(void) {
    mg_log_set(loglevel_to_mg_ll(get_log_level()));
#if MG_TLS == MG_TLS_MBED
    apply_mbedtls_debug_threshold();
#endif
}

static int mg_log_level_ticket = MG_LL_INFO;

void mg_log_prefix(int level, const char *file, int line, const char *fname) {
    (void)file;
    (void)line;
    (void)fname;
    mg_log_level_ticket = level;
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

#if defined(AVAR_TESTING)
void avar_mongoose_log_silence(void) {
    mg_log_set(MG_LL_NONE);
}
#endif
