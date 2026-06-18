#ifndef AVAR_STREAM_HLS_H
#define AVAR_STREAM_HLS_H

#include <stdbool.h>
#include <stddef.h>

/* Returns true when url or stream_kind indicates an HLS playlist download. */
bool stream_url_is_hls(const char *url, const char *stream_kind);

/*
 * Downloads an HLS playlist (AES-128 segments supported) into dest_path.
 * referer is sent as the Referer header when non-null.
 */
int stream_hls_download(const char *playlist_url, const char *dest_path, const char *referer);

#if defined(AVAR_TESTING)
bool stream_hls_test_playlist_is_master(const char *text);

char *stream_hls_test_pick_variant_url(const char *playlist, const char *base_url);

size_t stream_hls_test_parse_media_segment_count(const char *playlist, const char *base_url);

bool stream_hls_test_parse_hex_iv(const char *value, unsigned char iv[16]);
#endif

#endif
