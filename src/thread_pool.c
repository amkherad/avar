#include <thread_pool.h>

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <process.h>
    #include <windows.h>
#else
    #include <pthread.h>
#endif

typedef struct ThreadPoolWork {
    ThreadPoolTask task;
    void *arg;
    struct ThreadPoolWork *next;
} ThreadPoolWork;

struct ThreadPool {
#if defined(_WIN32)
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE work_available;
#else
    pthread_mutex_t lock;
    pthread_cond_t work_available;
#endif
    ThreadPoolWork *head;
    ThreadPoolWork *tail;
    size_t worker_count;
    bool shutdown;
#if defined(_WIN32)
    HANDLE *workers;
#else
    pthread_t *workers;
#endif
};

static ThreadPool *g_global_pool = NULL;

static void thread_pool_push_locked(ThreadPool *pool, ThreadPoolWork *work) {
    work->next = NULL;
    if (pool->tail == NULL) {
        pool->head = work;
        pool->tail = work;
    } else {
        pool->tail->next = work;
        pool->tail = work;
    }
}

static ThreadPoolWork *thread_pool_pop_locked(ThreadPool *pool) {
    ThreadPoolWork *work = pool->head;
    if (work != NULL) {
        pool->head = work->next;
        if (pool->head == NULL) {
            pool->tail = NULL;
        }
        work->next = NULL;
    }
    return work;
}

#if defined(_WIN32)
static unsigned __stdcall thread_pool_worker(void *arg) {
#else
static void *thread_pool_worker(void *arg) {
#endif
    ThreadPool *pool = (ThreadPool *)arg;
    if (pool == NULL) {
#if defined(_WIN32)
        return 0;
#else
        return NULL;
#endif
    }

    for (;;) {
#if defined(_WIN32)
        EnterCriticalSection(&pool->lock);
        while (pool->head == NULL && !pool->shutdown) {
            SleepConditionVariableCS(&pool->work_available, &pool->lock, INFINITE);
        }
#else
        pthread_mutex_lock(&pool->lock);
        while (pool->head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->work_available, &pool->lock);
        }
#endif

        if (pool->shutdown && pool->head == NULL) {
#if defined(_WIN32)
            LeaveCriticalSection(&pool->lock);
            return 0;
#else
            pthread_mutex_unlock(&pool->lock);
            return NULL;
#endif
        }

        ThreadPoolWork *work = thread_pool_pop_locked(pool);
#if defined(_WIN32)
        LeaveCriticalSection(&pool->lock);
#else
        pthread_mutex_unlock(&pool->lock);
#endif

        if (work != NULL) {
            if (work->task != NULL) {
                work->task(work->arg);
            }
            free(work);
        }
    }
}

ThreadPool *thread_pool_create(const size_t worker_count) {
    if (worker_count == 0) {
        return NULL;
    }

    ThreadPool *pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }

#if defined(_WIN32)
    InitializeCriticalSection(&pool->lock);
    InitializeConditionVariable(&pool->work_available);
    pool->workers = calloc(worker_count, sizeof(*pool->workers));
#else
    if (pthread_mutex_init(&pool->lock, NULL) != 0
        || pthread_cond_init(&pool->work_available, NULL) != 0) {
        free(pool);
        return NULL;
    }
    pool->workers = calloc(worker_count, sizeof(*pool->workers));
#endif

    if (pool->workers == NULL) {
        thread_pool_destroy(pool);
        return NULL;
    }

    pool->worker_count = worker_count;

    for (size_t i = 0; i < worker_count; i++) {
#if defined(_WIN32)
        pool->workers[i] =
                (HANDLE)_beginthreadex(NULL, 0, thread_pool_worker, pool, 0, NULL);
        if (pool->workers[i] == NULL) {
            pool->shutdown = true;
            WakeAllConditionVariable(&pool->work_available);
            thread_pool_destroy(pool);
            return NULL;
        }
#else
        if (pthread_create(&pool->workers[i], NULL, thread_pool_worker, pool) != 0) {
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->work_available);
            thread_pool_destroy(pool);
            return NULL;
        }
#endif
    }

    return pool;
}

void thread_pool_destroy(ThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    pool->shutdown = true;
#if defined(_WIN32)
    WakeAllConditionVariable(&pool->work_available);
#else
    pthread_cond_broadcast(&pool->work_available);
#endif

    if (pool->workers != NULL) {
        for (size_t i = 0; i < pool->worker_count; i++) {
#if defined(_WIN32)
            if (pool->workers[i] != NULL) {
                WaitForSingleObject(pool->workers[i], INFINITE);
                CloseHandle(pool->workers[i]);
            }
#else
            pthread_join(pool->workers[i], NULL);
#endif
        }
        free(pool->workers);
    }

    ThreadPoolWork *work = pool->head;
    while (work != NULL) {
        ThreadPoolWork *next = work->next;
        free(work);
        work = next;
    }

#if defined(_WIN32)
    DeleteCriticalSection(&pool->lock);
#else
    pthread_cond_destroy(&pool->work_available);
    pthread_mutex_destroy(&pool->lock);
#endif

    free(pool);
}

bool thread_pool_submit(ThreadPool *pool, ThreadPoolTask task, void *arg) {
    if (pool == NULL || task == NULL) {
        return false;
    }

    ThreadPoolWork *work = malloc(sizeof(*work));
    if (work == NULL) {
        return false;
    }

    work->task = task;
    work->arg = arg;
    work->next = NULL;

#if defined(_WIN32)
    EnterCriticalSection(&pool->lock);
    if (pool->shutdown) {
        LeaveCriticalSection(&pool->lock);
        free(work);
        return false;
    }
    thread_pool_push_locked(pool, work);
    WakeConditionVariable(&pool->work_available);
    LeaveCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        free(work);
        return false;
    }
    thread_pool_push_locked(pool, work);
    pthread_cond_signal(&pool->work_available);
    pthread_mutex_unlock(&pool->lock);
#endif

    return true;
}

ThreadPool *thread_pool_global(void) {
    if (g_global_pool == NULL) {
        g_global_pool = thread_pool_create(4U);
    }
    return g_global_pool;
}

#if defined(AVAR_TESTING)
void thread_pool_reset_global(void) {
    if (g_global_pool != NULL) {
        thread_pool_destroy(g_global_pool);
        g_global_pool = NULL;
    }
}

size_t thread_pool_worker_count(ThreadPool *pool) {
    return pool != NULL ? pool->worker_count : 0U;
}

size_t thread_pool_queue_depth(ThreadPool *pool) {
    if (pool == NULL) {
        return 0U;
    }

#if defined(_WIN32)
    EnterCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->lock);
#endif

    size_t depth = 0U;
    for (ThreadPoolWork *work = pool->head; work != NULL; work = work->next) {
        depth++;
    }

#if defined(_WIN32)
    LeaveCriticalSection(&pool->lock);
#else
    pthread_mutex_unlock(&pool->lock);
#endif

    return depth;
}
#endif
