#include <file_async.h>

#include <file-system.h>
#include <thread_pool.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

/* Sized to hold one full socket read (MG_IO_SIZE), so the steady state is one
 * buffer filling while the other is on the disk. Two buffers per open file is
 * the whole memory budget. */
#define ASYNC_FILE_BUFFER_SIZE (256U * 1024U)

typedef struct SwapBuffer {
    unsigned char *data;
    uint64_t offset; /* file offset of data[0] */
    size_t len;
} SwapBuffer;

struct AvarAsyncFile {
#if defined(_WIN32)
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE changed;
#else
    pthread_mutex_t lock;
    pthread_cond_t changed;
#endif
    FILE *fp;
    SwapBuffer bufs[2];
    int active;          /* buffer the producer fills */
    int pending;         /* buffer handed to the worker, -1 when none */
    bool worker_queued;  /* a pool task is scheduled or running */
    bool failed;         /* sticky write/flush error */
};

static void async_file_lock(AvarAsyncFile *file) {
#if defined(_WIN32)
    EnterCriticalSection(&file->lock);
#else
    pthread_mutex_lock(&file->lock);
#endif
}

static void async_file_unlock(AvarAsyncFile *file) {
#if defined(_WIN32)
    LeaveCriticalSection(&file->lock);
#else
    pthread_mutex_unlock(&file->lock);
#endif
}

static void async_file_wait(AvarAsyncFile *file) {
#if defined(_WIN32)
    SleepConditionVariableCS(&file->changed, &file->lock, INFINITE);
#else
    pthread_cond_wait(&file->changed, &file->lock);
#endif
}

static void async_file_wake(AvarAsyncFile *file) {
#if defined(_WIN32)
    WakeAllConditionVariable(&file->changed);
#else
    pthread_cond_broadcast(&file->changed);
#endif
}

static int async_file_seek(FILE *fp, const uint64_t offset) {
#if defined(_WIN32)
    return _fseeki64(fp, (__int64)offset, SEEK_SET);
#else
    return fseeko(fp, (off_t)offset, SEEK_SET);
#endif
}

/* Writes one buffer and flushes it to the OS. Runs on a pool worker (or
 * inline as a fallback). No locks held. */
static bool async_file_write_buffer(AvarAsyncFile *file, const SwapBuffer *buf) {
    if (async_file_seek(file->fp, buf->offset) != 0) {
        return false;
    }
    if (fwrite(buf->data, 1, buf->len, file->fp) != buf->len) {
        return false;
    }
    return fflush(file->fp) == 0;
}

static void async_file_worker(void *arg) {
    AvarAsyncFile *file = (AvarAsyncFile *)arg;

    async_file_lock(file);
    while (file->pending >= 0) {
        const SwapBuffer *buf = &file->bufs[file->pending];
        async_file_unlock(file);

        const bool ok = async_file_write_buffer(file, buf);

        async_file_lock(file);
        if (!ok) {
            file->failed = true;
        }
        file->pending = -1;
        async_file_wake(file);
    }
    file->worker_queued = false;
    async_file_wake(file);
    async_file_unlock(file);
}

/* Hands the active buffer to the worker and swaps to the free one. Waits for
 * the previous hand-off to finish first — this is the backpressure point.
 * Called with the lock held; returns with it held. */
static void async_file_submit_locked(AvarAsyncFile *file) {
    if (file->bufs[file->active].len == 0U) {
        return;
    }

    while (file->pending >= 0) {
        async_file_wait(file);
    }
    if (file->failed) {
        file->bufs[file->active].len = 0U;
        return;
    }

    file->pending = file->active;
    file->active ^= 1;
    file->bufs[file->active].len = 0U;

    if (!file->worker_queued) {
        if (thread_pool_submit(thread_pool_io_global(), async_file_worker, file)) {
            file->worker_queued = true;
        } else {
            /* Pool unavailable: write inline so no data is dropped. */
            const SwapBuffer *buf = &file->bufs[file->pending];
            if (!async_file_write_buffer(file, buf)) {
                file->failed = true;
            }
            file->pending = -1;
        }
    }
    async_file_wake(file);
}

AvarAsyncFile *avar_async_file_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        return NULL;
    }

    AvarAsyncFile *file = calloc(1, sizeof(*file));
    if (file == NULL) {
        return NULL;
    }

    file->bufs[0].data = malloc(ASYNC_FILE_BUFFER_SIZE);
    file->bufs[1].data = malloc(ASYNC_FILE_BUFFER_SIZE);
    file->fp = fopen(path, mode);
    if (file->bufs[0].data == NULL || file->bufs[1].data == NULL || file->fp == NULL) {
        if (file->fp != NULL) {
            fclose(file->fp);
        }
        free(file->bufs[0].data);
        free(file->bufs[1].data);
        free(file);
        return NULL;
    }

    file->pending = -1;
#if defined(_WIN32)
    InitializeCriticalSection(&file->lock);
    InitializeConditionVariable(&file->changed);
#else
    pthread_mutex_init(&file->lock, NULL);
    pthread_cond_init(&file->changed, NULL);
#endif
    return file;
}

int avar_async_file_write(AvarAsyncFile *file, uint64_t offset, const void *data, size_t len) {
    if (file == NULL || (data == NULL && len > 0U)) {
        return -1;
    }

    const unsigned char *cursor = data;

    async_file_lock(file);
    while (len > 0U && !file->failed) {
        SwapBuffer *buf = &file->bufs[file->active];

        /* A non-contiguous write ends the current buffer. */
        if (buf->len > 0U && offset != buf->offset + buf->len) {
            async_file_submit_locked(file);
            continue;
        }

        if (buf->len == 0U) {
            buf->offset = offset;
        }

        const size_t room = ASYNC_FILE_BUFFER_SIZE - buf->len;
        if (room == 0U) {
            async_file_submit_locked(file);
            continue;
        }

        const size_t chunk = len < room ? len : room;
        memcpy(buf->data + buf->len, cursor, chunk);
        buf->len += chunk;
        cursor += chunk;
        offset += chunk;
        len -= chunk;
    }
    const bool ok = !file->failed;
    async_file_unlock(file);

    return ok ? 0 : -1;
}

int avar_async_file_drain(AvarAsyncFile *file) {
    if (file == NULL) {
        return -1;
    }

    async_file_lock(file);
    async_file_submit_locked(file);
    while (file->pending >= 0) {
        async_file_wait(file);
    }
    const bool ok = !file->failed;
    async_file_unlock(file);

    return ok ? 0 : -1;
}

int avar_async_file_sync(AvarAsyncFile *file) {
    if (avar_async_file_drain(file) != 0) {
        return -1;
    }
    if (file_sync_to_disk(file->fp) != 0) {
        async_file_lock(file);
        file->failed = true;
        async_file_unlock(file);
        return -1;
    }
    return 0;
}

int avar_async_file_error(AvarAsyncFile *file) {
    if (file == NULL) {
        return -1;
    }

    async_file_lock(file);
    const bool ok = !file->failed;
    async_file_unlock(file);

    return ok ? 0 : -1;
}

int avar_async_file_close(AvarAsyncFile *file) {
    if (file == NULL) {
        return 0;
    }

    const int rc = avar_async_file_sync(file);

    /* The worker has finished (drain waited for it), but it may still hold
     * the lock between pending = -1 and returning; take the lock once so the
     * primitives are not destroyed under it. */
    async_file_lock(file);
    while (file->worker_queued) {
        async_file_wait(file);
    }
    async_file_unlock(file);

    const int close_rc = fclose(file->fp);

#if defined(_WIN32)
    DeleteCriticalSection(&file->lock);
#else
    pthread_cond_destroy(&file->changed);
    pthread_mutex_destroy(&file->lock);
#endif
    free(file->bufs[0].data);
    free(file->bufs[1].data);
    free(file);

    return rc == 0 && close_rc == 0 ? 0 : -1;
}
