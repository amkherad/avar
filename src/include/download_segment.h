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

typedef bool (*SegmentRangePredicate)(const void *ctx, uint64_t start, uint64_t end);

bool segment_ranges_overlap(uint64_t a_start, uint64_t a_end, uint64_t b_start, uint64_t b_end);

/* Returns the inclusive end of the next chunk-sized reservation after current_end,
 * or current_end when the next buffer is blocked by done or in-flight ranges. */
uint64_t segment_next_reserve_end(const DownloadState *state, const ByteRange *in_flight,
                                  size_t in_flight_count, uint64_t current_end);

void segment_index_range(uint64_t total_size, size_t chunk_size, size_t segment_index,
                         uint64_t *start_out, uint64_t *end_out);

void segment_chunk_range(const DownloadState *state, size_t segment_index, uint64_t *start_out,
                         uint64_t *end_out);

size_t segment_select_balanced(const DownloadState *state, size_t concurrency, size_t max_count,
                               SegmentRangePredicate predicate, const void *predicate_ctx,
                               ByteRange *out_ranges);

size_t segment_select_left_heavy(const DownloadState *state, size_t max_count,
                                 SegmentRangePredicate predicate, const void *predicate_ctx,
                                 ByteRange *out_ranges);

size_t segment_select_chunks(const DownloadSegmentConfig *cfg, const DownloadState *state,
                             size_t max_count, SegmentRangePredicate predicate,
                             const void *predicate_ctx, ByteRange *out_ranges);

SegmentStrategy segment_strategy_from_string(const char *value);

bool segment_should_enable(const DownloadSegmentConfig *cfg, uint64_t total_size,
                           bool ranges_supported);

#endif
