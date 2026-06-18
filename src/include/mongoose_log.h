#ifndef AVAR_MONGOOSE_LOG_H
#define AVAR_MONGOOSE_LOG_H

void avar_mongoose_log_init(void);

#if defined(AVAR_TESTING)
void avar_mongoose_log_silence(void);
#endif

#endif
