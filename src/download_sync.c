#include <download_sync.h>

#include <stdlib.h>

#if defined(_WIN32)
    #include <windows.h>
struct AvarMutex {
    CRITICAL_SECTION cs;
};
#else
    #include <pthread.h>
struct AvarMutex {
    pthread_mutex_t mutex;
};
#endif

AvarMutex *avar_mutex_create(void) {
    AvarMutex *mutex = malloc(sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }

#if defined(_WIN32)
    InitializeCriticalSection(&mutex->cs);
#else
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
#endif

    return mutex;
}

void avar_mutex_destroy(AvarMutex *mutex) {
    if (mutex == NULL) {
        return;
    }

#if defined(_WIN32)
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    free(mutex);
}

void avar_mutex_lock(AvarMutex *mutex) {
    if (mutex == NULL) {
        return;
    }

#if defined(_WIN32)
    EnterCriticalSection(&mutex->cs);
#else
    pthread_mutex_lock(&mutex->mutex);
#endif
}

void avar_mutex_unlock(AvarMutex *mutex) {
    if (mutex == NULL) {
        return;
    }

#if defined(_WIN32)
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
}
