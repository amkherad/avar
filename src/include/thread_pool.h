#ifndef AVAR_THREAD_POOL_H
#define AVAR_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*ThreadPoolTask)(void *arg);

typedef struct ThreadPool ThreadPool;

ThreadPool *thread_pool_create(size_t worker_count);

void thread_pool_destroy(ThreadPool *pool);

bool thread_pool_submit(ThreadPool *pool, ThreadPoolTask task, void *arg);

ThreadPool *thread_pool_global(void);

/**
 * Dedicated pool for async file-write flush tasks (see file_async.c). Kept
 * separate from thread_pool_global() so that saturating the download-job
 * pool (one long-lived worker per active download) can never starve the
 * short-lived write tasks those same downloads depend on to make progress.
 */
ThreadPool *thread_pool_io_global(void);

#if defined(AVAR_TESTING)
void thread_pool_reset_global(void);

size_t thread_pool_worker_count(ThreadPool *pool);

size_t thread_pool_queue_depth(ThreadPool *pool);
#endif

#endif
