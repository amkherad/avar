#include "mongoose_log.h"

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
#endif
static void avar_test_global_setup(void) {
    avar_mongoose_log_silence();
}
