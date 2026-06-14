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

#endif
