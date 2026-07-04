#ifndef AVAR_FILE_ASYNC_H
#define AVAR_FILE_ASYNC_H

#include <stddef.h>
#include <stdint.h>

/* Asynchronous file writer.
 *
 * Decouples the network transfer loop from disk latency: writes are copied
 * into one of two fixed swap buffers and performed on the global thread pool,
 * so the producer never waits for the disk unless it gets more than one full
 * buffer ahead of it (backpressure keeps memory bounded at two buffers per
 * file). Contiguous writes coalesce into the active buffer; a write to a
 * different offset hands the buffer to the worker and swaps.
 *
 * Single producer: all calls except the internal worker must come from one
 * thread at a time (callers serialize with their own lock, as the download
 * engine does with job->mutex).
 *
 * Errors are sticky: once a disk write fails, every later call returns -1
 * until the file is closed. Callers must treat a failed close as data loss. */

typedef struct AvarAsyncFile AvarAsyncFile;

/* Opens path with fopen() semantics ("wb", "r+b", "ab", ...). Returns NULL on
 * failure. In append modes the offsets passed to write must be the current
 * end of file. */
AvarAsyncFile *avar_async_file_open(const char *path, const char *mode);

/* Queues len bytes for writing at offset. Copies data; the caller may reuse
 * its buffer immediately. Blocks only when both swap buffers are full.
 * Returns 0 on success, -1 on (sticky) error. */
int avar_async_file_write(AvarAsyncFile *file, uint64_t offset, const void *data, size_t len);

/* Blocks until every queued byte has been written and fflush()ed to the OS.
 * Call before persisting state that describes the data. Returns 0 on success,
 * -1 on (sticky) error. */
int avar_async_file_drain(AvarAsyncFile *file);

/* Drains, then forces the file to physical storage (fsync). Returns 0 on
 * success, -1 on (sticky) error. */
int avar_async_file_sync(AvarAsyncFile *file);

/* Returns 0 when no write has failed so far, -1 otherwise. Does not block. */
int avar_async_file_error(AvarAsyncFile *file);

/* Drains, fsyncs, closes and frees the file. Returns 0 only when every queued
 * byte reached the disk. file is invalid afterwards (NULL is a no-op). */
int avar_async_file_close(AvarAsyncFile *file);

#endif
