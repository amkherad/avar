#ifndef AVAR_STREAM_HLS_H
#define AVAR_STREAM_HLS_H

#include <stdbool.h>

/* Returns true when url or stream_kind indicates an HLS playlist download. */
bool stream_url_is_hls(const char *url, const char *stream_kind);

/*
 * Downloads an HLS playlist (AES-128 segments supported) into dest_path.
 * referer is sent as the Referer header when non-null.
 */
int stream_hls_download(const char *playlist_url, const char *dest_path, const char *referer);

#endif
