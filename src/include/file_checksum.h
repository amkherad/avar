#ifndef AVAR_FILE_CHECKSUM_H
#define AVAR_FILE_CHECKSUM_H

#include "avar.h"

#include <stdbool.h>
#include <stddef.h>

/** Supported checksum algorithm names (lowercase). */
bool file_checksum_algorithm_supported(const char *algorithm);

/**
 * Computes a lowercase hex digest for path using algorithm.
 * Writes a newly allocated string to hex_out on success. Caller must free(*hex_out).
 */
int file_checksum_file(const char *path, const char *algorithm, char **hex_out);

/** Normalizes a checksum string for comparison (lowercase hex, no separators). */
void file_checksum_normalize(const char *value, char *out, size_t out_len);

/** Returns true when normalized checksum strings match. */
bool file_checksum_matches(const char *computed, const char *expected);

#endif
