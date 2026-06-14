#include <download_segment.h>

#include <avar.h>

#include <stdlib.h>
#include <string.h>

void segment_chunk_range(const DownloadState *state, const size_t chunk_index, uint64_t *start_out,
                         uint64_t *end_out) {
    if (start_out == NULL || end_out == NULL || state == NULL) {
        return;
    }

    const uint64_t start = (uint64_t)chunk_index * (uint64_t)state->chunk_size;
    uint64_t end = start + (uint64_t)state->chunk_size - 1U;
    if (state->total_size > 0 && end >= state->total_size) {
        end = state->total_size - 1U;
    }

    *start_out = start;
    *end_out = end;
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

    if (total_size == 0 || total_size < cfg->min_file_size) {
        return false;
    }

    return true;
}

size_t segment_select_left_heavy(const DownloadState *state, const size_t max_count,
                                 SegmentChunkPredicate predicate, const void *predicate_ctx,
                                 size_t *out_indices) {
    if (state == NULL || state->chunks_done == NULL || max_count == 0 || out_indices == NULL) {
        return 0;
    }

    size_t selected = 0;
    for (size_t i = 0; i < state->chunk_count && selected < max_count; i++) {
        if (state->chunks_done[i]) {
            continue;
        }

        if (predicate != NULL && !predicate(predicate_ctx, i)) {
            continue;
        }

        out_indices[selected++] = i;
    }

    return selected;
}

size_t segment_select_balanced(const DownloadState *state, const size_t concurrency,
                               const size_t max_count, SegmentChunkPredicate predicate,
                               const void *predicate_ctx, size_t *out_indices) {
    if (state == NULL || state->chunks_done == NULL || max_count == 0 || out_indices == NULL
        || concurrency == 0) {
        return 0;
    }

    size_t selected = 0;
    const size_t stride = concurrency > 0 ? concurrency : 1U;

    for (size_t bucket = 0; bucket < stride && selected < max_count; bucket++) {
        for (size_t idx = bucket; idx < state->chunk_count; idx += stride) {
            if (state->chunks_done[idx]) {
                continue;
            }

            if (predicate != NULL && !predicate(predicate_ctx, idx)) {
                continue;
            }

            out_indices[selected++] = idx;
            break;
        }
    }

    if (selected < max_count) {
        for (size_t i = 0; i < state->chunk_count && selected < max_count; i++) {
            if (state->chunks_done[i]) {
                continue;
            }

            if (predicate != NULL && !predicate(predicate_ctx, i)) {
                continue;
            }

            bool already_selected = false;
            for (size_t j = 0; j < selected; j++) {
                if (out_indices[j] == i) {
                    already_selected = true;
                    break;
                }
            }

            if (!already_selected) {
                out_indices[selected++] = i;
            }
        }
    }

    return selected;
}

size_t segment_select_chunks(const DownloadSegmentConfig *cfg, const DownloadState *state,
                             const size_t max_count, SegmentChunkPredicate predicate,
                             const void *predicate_ctx, size_t *out_indices) {
    if (cfg == NULL) {
        return 0;
    }

    if (cfg->strategy == SegmentStrategyLeftHeavy) {
        return segment_select_left_heavy(state, max_count, predicate, predicate_ctx, out_indices);
    }

    return segment_select_balanced(state, cfg->concurrency, max_count, predicate, predicate_ctx,
                                 out_indices);
}
