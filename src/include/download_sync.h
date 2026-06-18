#ifndef AVAR_DOWNLOAD_SYNC_H
#define AVAR_DOWNLOAD_SYNC_H

#include <stdbool.h>

typedef struct AvarMutex AvarMutex;

AvarMutex *avar_mutex_create(void);

void avar_mutex_destroy(AvarMutex *mutex);

void avar_mutex_lock(AvarMutex *mutex);

void avar_mutex_unlock(AvarMutex *mutex);

#endif
