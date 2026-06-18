#include "avar_test.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread_pool.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

typedef struct {
    atomic_int value;
} CounterArg;

static void increment_task(void *arg) {
    CounterArg *counter = (CounterArg *)arg;
    if (counter != NULL) {
        atomic_fetch_add(&counter->value, 1);
    }
}

typedef struct {
    atomic_int running;
    atomic_int max_running;
    atomic_int completed;
} ConcurrentCtx;

static void concurrent_task(void *arg) {
    ConcurrentCtx *ctx = (ConcurrentCtx *)arg;
    if (ctx == NULL) {
        return;
    }

    const int active = atomic_fetch_add(&ctx->running, 1) + 1;
    int observed = atomic_load(&ctx->max_running);
    while (active > observed
           && !atomic_compare_exchange_weak(&ctx->max_running, &observed, active)) {
    }

#if defined(_WIN32)
    Sleep(50);
#else
    usleep(50000);
#endif

    atomic_fetch_sub(&ctx->running, 1);
    atomic_fetch_add(&ctx->completed, 1);
}

AVAR_TEST(thread_pool_create_requires_workers) {
    AVAR_ASSERT_NULL(thread_pool_create(0U));
}

AVAR_TEST(thread_pool_submit_executes_task) {
    ThreadPool *pool = thread_pool_create(2U);
    AVAR_ASSERT_NOT_NULL(pool);

    CounterArg counter = {.value = 0};
    AVAR_ASSERT(thread_pool_submit(pool, increment_task, &counter));

#if defined(_WIN32)
    Sleep(200);
#else
    usleep(200000);
#endif

    AVAR_ASSERT_EQ(atomic_load(&counter.value), 1);
    thread_pool_destroy(pool);
}

AVAR_TEST(thread_pool_runs_tasks_concurrently) {
    ThreadPool *pool = thread_pool_create(4U);
    AVAR_ASSERT_NOT_NULL(pool);

    ConcurrentCtx ctx = {
            .running = 0,
            .max_running = 0,
            .completed = 0,
    };

    for (int i = 0; i < 8; i++) {
        AVAR_ASSERT(thread_pool_submit(pool, concurrent_task, &ctx));
    }

#if defined(_WIN32)
    Sleep(500);
#else
    usleep(500000);
#endif

    AVAR_ASSERT_EQ(atomic_load(&ctx.completed), 8);
    AVAR_ASSERT(atomic_load(&ctx.max_running) >= 2);
    thread_pool_destroy(pool);
}

AVAR_TEST(thread_pool_many_submissions_complete) {
    ThreadPool *pool = thread_pool_create(4U);
    AVAR_ASSERT_NOT_NULL(pool);

    CounterArg counter = {.value = 0};
    for (int i = 0; i < 64; i++) {
        AVAR_ASSERT(thread_pool_submit(pool, increment_task, &counter));
    }

#if defined(_WIN32)
    Sleep(1000);
#else
    usleep(1000000);
#endif

    AVAR_ASSERT_EQ(atomic_load(&counter.value), 64);
    thread_pool_destroy(pool);
}

AVAR_TEST(thread_pool_submit_rejects_null_pool) {
    AVAR_ASSERT(!thread_pool_submit(NULL, increment_task, NULL));
}

AVAR_TEST(thread_pool_global_is_reused) {
    thread_pool_reset_global();
    ThreadPool *first = thread_pool_global();
    ThreadPool *second = thread_pool_global();
    AVAR_ASSERT_NOT_NULL(first);
    AVAR_ASSERT_EQ(first, second);
    AVAR_ASSERT_EQ(thread_pool_worker_count(first), 4U);
    thread_pool_reset_global();
}

AVAR_TEST_MAIN(
        run_thread_pool_create_requires_workers();
        run_thread_pool_submit_executes_task();
        run_thread_pool_runs_tasks_concurrently();
        run_thread_pool_many_submissions_complete();
        run_thread_pool_submit_rejects_null_pool();
        run_thread_pool_global_is_reused();)
