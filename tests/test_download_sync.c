#include "avar_test.h"

#include <download_sync.h>

#include <stdatomic.h>
#include <stdlib.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

typedef struct {
    AvarMutex *mutex;
    atomic_int counter;
} CounterCtx;

#if defined(_WIN32)
static DWORD WINAPI increment_thread(LPVOID arg) {
#else
static void *increment_thread(void *arg) {
#endif
    CounterCtx *ctx = (CounterCtx *)arg;
    for (int i = 0; i < 1000; i++) {
        avar_mutex_lock(ctx->mutex);
        atomic_fetch_add(&ctx->counter, 1);
        avar_mutex_unlock(ctx->mutex);
    }
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

AVAR_TEST(download_sync_mutex_serializes_access) {
    AvarMutex *mutex = avar_mutex_create();
    AVAR_ASSERT_NOT_NULL(mutex);

    CounterCtx ctx = {.mutex = mutex, .counter = 0};

#if defined(_WIN32)
    HANDLE threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = CreateThread(NULL, 0, increment_thread, &ctx, 0, NULL);
        AVAR_ASSERT_NOT_NULL(threads[i]);
    }
    WaitForMultipleObjects(4, threads, TRUE, INFINITE);
    for (int i = 0; i < 4; i++) {
        CloseHandle(threads[i]);
    }
#else
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        AVAR_ASSERT_EQ(pthread_create(&threads[i], NULL, increment_thread, &ctx), 0);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    AVAR_ASSERT_EQ(atomic_load(&ctx.counter), 4000);
    avar_mutex_destroy(mutex);
}

AVAR_TEST(download_sync_null_mutex_is_safe) {
    avar_mutex_lock(NULL);
    avar_mutex_unlock(NULL);
    avar_mutex_destroy(NULL);
}

AVAR_TEST_MAIN(
        run_download_sync_mutex_serializes_access();
        run_download_sync_null_mutex_is_safe();)
