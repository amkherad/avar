#include <download_segment.h>

#include <avar.h>

#include <stdlib.h>
#include <string.h>

void segment_index_range(const uint64_t total_size, const size_t chunk_size,
                         const size_t segment_index, uint64_t *start_out, uint64_t *end_out) {
    if (start_out == NULL || end_out == NULL) {
        return;
    }

    const size_t segment_size = chunk_size > 0U ? chunk_size : DL_CHUNK_SIZE;
    const uint64_t start = (uint64_t)segment_index * (uint64_t)segment_size;
    uint64_t end = start + (uint64_t)segment_size - 1U;
    if (total_size > 0U && end >= total_size) {
        end = total_size - 1U;
    }

    *start_out = start;
    *end_out = end;
}

void segment_chunk_range(const DownloadState *state, const size_t segment_index,
                         uint64_t *start_out, uint64_t *end_out) {
    if (state == NULL) {
        return;
    }

    const size_t chunk_size = state->chunk_size > 0U ? state->chunk_size : DL_CHUNK_SIZE;
    segment_index_range(state->total_size, chunk_size, segment_index, start_out, end_out);
}

SegmentStrategy segment_strategy_from_string(const char *value) {
    if (value == NULL) {
        return SegmentStrategyBalanced;
    }

    if (strcmp(value, AVAR_SEGMENT_STRATEGY_LEFT_HEAVY) == 0) {
        return SegmentStrategyLeftHeavy;
    }

    return SegmentStrategyBalanced;
}

bool segment_should_enable(const DownloadSegmentConfig *cfg, const uint64_t total_size,
                           const bool ranges_supported) {
    if (cfg == NULL || !cfg->enabled || !ranges_supported) {
        return false;
    }

    if (cfg->concurrency <= 1U) {
        return false;
    }

    if (total_size == 0U || total_size < cfg->min_file_size) {
        return false;
    }

    return true;
}

static bool range_selected(const ByteRange *ranges, const size_t count, const uint64_t start,
                           const uint64_t end) {
    for (size_t i = 0U; i < count; i++) {
        if (ranges[i].start == start && ranges[i].end == end) {
            return true;
        }
    }
    return false;
}

static bool try_select_segment(const DownloadState *state, const size_t segment_index,
                              SegmentRangePredicate predicate, const void *predicate_ctx,
                              ByteRange *out_ranges, size_t *selected) {
    if (download_state_is_segment_done(state, segment_index)) {
        return false;
    }

    uint64_t start = 0U;
    uint64_t end = 0U;
    segment_chunk_range(state, segment_index, &start, &end);

    if (predicate != NULL && !predicate(predicate_ctx, start, end)) {
        return false;
    }

    out_ranges[*selected].start = start;
    out_ranges[*selected].end = end;
    (*selected)++;
    return true;
}

size_t segment_select_left_heavy(const DownloadState *state, const size_t max_count,
                                 SegmentRangePredicate predicate, const void *predicate_ctx,
                                 ByteRange *out_ranges) {
    if (state == NULL || max_count == 0U || out_ranges == NULL || state->total_size == 0U) {
        return 0U;
    }

    const size_t segment_count = download_state_segment_count(state);
    size_t selected = 0U;
    for (size_t i = 0U; i < segment_count && selected < max_count; i++) {
        (void)try_select_segment(state, i, predicate, predicate_ctx, out_ranges, &selected);
    }

    return selected;
}

size_t segment_select_balanced(const DownloadState *state, const size_t concurrency,
                               const size_t max_count, SegmentRangePredicate predicate,
                               const void *predicate_ctx, ByteRange *out_ranges) {
    if (state == NULL || max_count == 0U || out_ranges == NULL || concurrency == 0U
        || state->total_size == 0U) {
        return 0U;
    }

    const size_t segment_count = download_state_segment_count(state);
    if (segment_count == 0U) {
        return 0U;
    }

    size_t selected = 0U;
    const size_t bucket_count = concurrency < segment_count ? concurrency : segment_count;
    const size_t bucket_size = (segment_count + bucket_count - 1U) / bucket_count;

    for (size_t bucket = 0U; bucket < bucket_count && selected < max_count; bucket++) {
        const size_t bucket_start = bucket * bucket_size;
        const size_t bucket_end = bucket_start + bucket_size < segment_count
                                          ? bucket_start + bucket_size
                                          : segment_count;

        for (size_t idx = bucket_start; idx < bucket_end; idx++) {
            if (try_select_segment(state, idx, predicate, predicate_ctx, out_ranges, &selected)) {
                break;
            }
        }
    }

    if (selected < max_count) {
        for (size_t i = 0U; i < segment_count && selected < max_count; i++) {
            if (download_state_is_segment_done(state, i)) {
                continue;
            }

            uint64_t start = 0U;
            uint64_t end = 0U;
            segment_chunk_range(state, i, &start, &end);

            if (predicate != NULL && !predicate(predicate_ctx, start, end)) {
                continue;
            }

            if (range_selected(out_ranges, selected, start, end)) {
                continue;
            }

            out_ranges[selected].start = start;
            out_ranges[selected].end = end;
            selected++;
        }
    }

    return selected;
}

size_t segment_select_chunks(const DownloadSegmentConfig *cfg, const DownloadState *state,
                             const size_t max_count, SegmentRangePredicate predicate,
                             const void *predicate_ctx, ByteRange *out_ranges) {
    if (cfg == NULL) {
        return 0U;
    }

    if (cfg->strategy == SegmentStrategyLeftHeavy) {
        return segment_select_left_heavy(state, max_count, predicate, predicate_ctx, out_ranges);
    }

    return segment_select_balanced(state, cfg->concurrency, max_count, predicate, predicate_ctx,
                                   out_ranges);
}
