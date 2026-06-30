#ifndef AVAR_DOWNLOAD_PROBE_H
#define AVAR_DOWNLOAD_PROBE_H

#include <stdint.h>

typedef struct {
    int exit_code;
    int http_status;
    uint64_t total_bytes;
} DownloadProbeResult;

/**
 * Probes an HTTP(S) resource for total size without downloading the body.
 * When proxy_url is NULL and download_id is set, the download item proxy is used.
 */
int download_probe_url(const char *url, const char *referer, const char *proxy_url,
                       const char *download_id, DownloadProbeResult *out);

#endif
