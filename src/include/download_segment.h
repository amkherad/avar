#ifndef AVAR_DOWNLOAD_SEGMENT_H
#define AVAR_DOWNLOAD_SEGMENT_H

#include "download_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SegmentStrategyBalanced,
    SegmentStrategyLeftHeavy,
} SegmentStrategy;

typedef struct DownloadSegmentConfig {
    bool enabled;
    SegmentStrategy strategy;
    size_t concurrency;
    size_t chunk_size;
    uint64_t min_file_size;
    double left_heavy_front_ratio;
} DownloadSegmentConfig;

typedef bool (*SegmentChunkPredicate)(const void *ctx, size_t chunk_index);

void segment_chunk_range(const DownloadState *state, size_t chunk_index, uint64_t *start_out,
                         uint64_t *end_out);

size_t segment_select_balanced(const DownloadState *state, size_t concurrency, size_t max_count,
                               SegmentChunkPredicate predicate, const void *predicate_ctx,
                               size_t *out_indices);

size_t segment_select_left_heavy(const DownloadState *state, size_t max_count,
                                 SegmentChunkPredicate predicate, const void *predicate_ctx,
                                 size_t *out_indices);

size_t segment_select_chunks(const DownloadSegmentConfig *cfg, const DownloadState *state,
                             size_t max_count, SegmentChunkPredicate predicate,
                             const void *predicate_ctx, size_t *out_indices);

SegmentStrategy segment_strategy_from_string(const char *value);

bool segment_should_enable(const DownloadSegmentConfig *cfg, uint64_t total_size,
                           bool ranges_supported);

#endif
