#ifndef AVAR_H
#define AVAR_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define STR(x) #x
#define TOSTR(x) STR(x)

/* -------------------------------------------------------------------------- */
/* Application identity                                                       */
/* -------------------------------------------------------------------------- */

#define APP_NAME "Avar"
#define APP_ID "avar"

#if defined(__has_include)
#  if __has_include("avar_version.h")
#    include "avar_version.h"
#  endif
#endif
#ifndef VERSION_STR
#define VERSION_STR "0.0.0-dev"
#endif

/* -------------------------------------------------------------------------- */
/* Exit codes                                                                 */
/* -------------------------------------------------------------------------- */

#define EXIT_UNKNOWN_COMMAND 2
#define EXIT_FATAL_ERROR 3

/* -------------------------------------------------------------------------- */
/* IEC binary prefixes                                                        */
/* KiB/MiB/GiB = kibibytes (1024^n bytes); Kib/Mib/Gib = kibibits (1024^n bits) */
/* -------------------------------------------------------------------------- */

#define AVAR_KIB 1024U
#define AVAR_MIB (AVAR_KIB * AVAR_KIB)
#define AVAR_GIB (AVAR_MIB * AVAR_KIB)
#define AVAR_BITS_PER_BYTE 8U

/* -------------------------------------------------------------------------- */
/* Data-size unit labels (kibibytes)                                            */
/* -------------------------------------------------------------------------- */

#define AVAR_UNIT_BYTES "Bytes"
#define AVAR_UNIT_KIB "KiB"
#define AVAR_UNIT_MIB "MiB"
#define AVAR_UNIT_GIB "GiB"

/* -------------------------------------------------------------------------- */
/* Transfer-rate unit labels                                                  */
/* -------------------------------------------------------------------------- */

#define AVAR_UNIT_BYTES_PER_SEC "Bytes/s"
#define AVAR_UNIT_BITS_PER_SEC "bits/s"
#define AVAR_UNIT_KIB_PER_SEC "KiB/s"
#define AVAR_UNIT_MIB_PER_SEC "MiB/s"
#define AVAR_UNIT_GIB_PER_SEC "GiB/s"
#define AVAR_UNIT_KIBIT_PER_SEC "Kib/s"
#define AVAR_UNIT_MIBIT_PER_SEC "Mib/s"
#define AVAR_UNIT_GIBIT_PER_SEC "Gib/s"

#define AVAR_DEFAULT_SIZE_UNIT AVAR_UNIT_MIB
#define AVAR_DEFAULT_SPEED_UNIT AVAR_UNIT_MIB_PER_SEC

/* -------------------------------------------------------------------------- */
/* Paths                                                                      */
/* -------------------------------------------------------------------------- */

#if defined(_WIN32)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#define AVAR_CONFIG_FILENAME "config.json"
#define AVAR_CONFIG_TMP_SUFFIX ".tmp"
#define AVAR_CONFIG_PATH_MAX 256U

/* Environment variables override config.json: avar.<config-key> (e.g. avar.daemon.session.mode). */
#define AVAR_ENV_PREFIX "avar."

#define AVAR_DIR_DOWNLOAD_TEMP "download-temp"
#define AVAR_DIR_DOWNLOADS "Downloads"
#define AVAR_DIR_LOCAL_SHARE ".local/share"

#define AVAR_SOCK_DEFAULT "/tmp/avar.sock"

/* -------------------------------------------------------------------------- */
/* Environment variable names                                                 */
/* -------------------------------------------------------------------------- */

#define AVAR_ENV_HOME "HOME"
#define AVAR_ENV_USERPROFILE "USERPROFILE"
#define AVAR_ENV_APPDATA "APPDATA"
#define AVAR_ENV_XDG_CONFIG_HOME "XDG_CONFIG_HOME"
#define AVAR_ENV_XDG_DATA_HOME "XDG_DATA_HOME"
#define AVAR_ENV_XDG_DOWNLOAD_DIR "XDG_DOWNLOAD_DIR"
#define AVAR_ENV_TEMP "TEMP"
#define AVAR_ENV_INSTANCE "AVAR_INSTANCE"

/* -------------------------------------------------------------------------- */
/* Config keys                                                                */
/* -------------------------------------------------------------------------- */

#define AVAR_CFG_DM_ITEMS "dm.items"
#define AVAR_CFG_DM_QUEUES "dm.queues"
#define AVAR_CFG_DM_TEMP_PATH "dm.tempPath"
#define AVAR_CFG_DM_DOWNLOAD_PATH "dm.downloadPath"
#define AVAR_CFG_DM_PROGRESS_SIZE_UNIT "dm.progress.sizeUnit"
#define AVAR_CFG_DM_PROGRESS_SPEED_UNIT "dm.progress.speedUnit"
#define AVAR_CFG_DM_PROGRESS_STYLE "dm.progress.style"

#define AVAR_PROGRESS_STYLE_AGGREGATE "aggregate"
#define AVAR_PROGRESS_STYLE_SEGMENTED "segmented"
#define AVAR_DEFAULT_PROGRESS_STYLE AVAR_PROGRESS_STYLE_SEGMENTED

#define AVAR_CFG_DM_SEGMENTATION "dm.segmentation"
#define AVAR_CFG_DM_SEGMENTATION_ENABLED "dm.segmentation.enabled"
#define AVAR_CFG_DM_SEGMENTATION_STRATEGY "dm.segmentation.strategy"
#define AVAR_CFG_DM_SEGMENTATION_CONCURRENCY "dm.segmentation.concurrency"
#define AVAR_CFG_DM_SEGMENTATION_CHUNK_SIZE "dm.segmentation.chunkSize"
#define AVAR_CFG_DM_SEGMENTATION_MIN_FILE_SIZE "dm.segmentation.minFileSize"
#define AVAR_CFG_DM_SEGMENTATION_LEFT_HEAVY_RATIO "dm.segmentation.leftHeavyFrontRatio"

#define AVAR_CFG_DM_PROXY "dm.proxy"
#define AVAR_CFG_DM_PROXY_ENABLED "dm.proxy.enabled"
#define AVAR_CFG_DM_PROXY_TYPE "dm.proxy.type"
#define AVAR_CFG_DM_PROXY_HOST "dm.proxy.host"
#define AVAR_CFG_DM_PROXY_PORT "dm.proxy.port"
#define AVAR_CFG_DM_PROXY_USERNAME "dm.proxy.username"
#define AVAR_CFG_DM_PROXY_PASSWORD "dm.proxy.password"
#define AVAR_CFG_DM_PROXY_NO_PROXY "dm.proxy.noProxy"

#define AVAR_CFG_LOG_FILE "log.file"
#define AVAR_CFG_LOG_FILE_ENABLED "log.file.enabled"
#define AVAR_CFG_LOG_FILE_PATH "log.file.path"

#define AVAR_SEGMENT_STRATEGY_BALANCED "balanced"
#define AVAR_SEGMENT_STRATEGY_LEFT_HEAVY "left-heavy"

#define DL_DEFAULT_SEGMENT_CONCURRENCY 4U
#define DL_DEFAULT_MAX_CONCURRENT_DOWNLOADS 4U
#define DL_DEFAULT_MIN_SEGMENT_FILE_SIZE AVAR_MIB
#define DL_DEFAULT_LEFT_HEAVY_FRONT_RATIO 0.25

/* -------------------------------------------------------------------------- */
/* Download manager                                                           */
/* -------------------------------------------------------------------------- */

#define DL_STATE_FILENAME "state.json"
#define DL_CHUNK_SIZE (256U * AVAR_KIB)
#if defined(AVAR_TESTING)
#define DL_CONNECT_TIMEOUT_MS 5000U
#define DL_IDLE_TIMEOUT_MS 20000U
#define AVAR_TEST_DOWNLOAD_MAX_MS 90000U
#else
#define DL_CONNECT_TIMEOUT_MS 30000U
#define DL_IDLE_TIMEOUT_MS 120000U
#endif
/* Allowed segment connection failures before the whole download is abandoned,
 * expressed as a multiple of the segment count (retries are per-segment). */
#define DL_SEGMENT_MAX_RETRY_FACTOR 4U
#define DL_DEFAULT_MAX_RETRIES 10U
#define DL_MAX_REDIRECTS 10
#define DL_WRITE_CHUNK_SIZE AVAR_MIB
#define DL_PROGRESS_BAR_WIDTH 22
#define DL_PROGRESS_BAR_WIDTH_MIN 10
#define DL_PROGRESS_BAR_WIDTH_MAX 400
#define DL_PROGRESS_PERCENT_WIDTH 3
#define DL_PROGRESS_SPEED_NUMBER_WIDTH 6
#define DL_PROGRESS_LINE_BUF_SIZE 512U
#define DL_PROGRESS_PERCENT_MAX 100
#define DL_POLL_MS 50U

#define AVAR_QUEUE_ID_PREFIX "queue-"
#define AVAR_QUEUE_ID_BUF_SIZE 64U
#define AVAR_QUEUE_NAME_MAX 128U

#define AVAR_DL_ID_PREFIX "dl-"
#define AVAR_DOWNLOAD_FALLBACK_PREFIX "download-"
#define AVAR_DOWNLOAD_FALLBACK_SUFFIX ".bin"

#define AVAR_DL_STATUS_COMPLETED "completed"
#define AVAR_DL_STATUS_DOWNLOADING "downloading"
#define AVAR_DL_STATUS_FAILED "failed"
#define AVAR_DL_STATUS_QUEUED "queued"
#define AVAR_DL_STATUS_PAUSED "paused"
#define AVAR_DL_STATUS_STOPPED "stopped"

/** Machine-readable description when a partial download cannot be resumed. */
#define AVAR_DL_DESC_RESUME_UNSUPPORTED "resume_unsupported"

#define AVAR_DL_ADDED_DIRECT "direct"

#define AVAR_PROXY_TYPE_HTTP "http"
#define AVAR_PROXY_TYPE_HTTPS "https"
#define AVAR_PROXY_TYPE_SOCKS5 "socks5"

/* -------------------------------------------------------------------------- */
/* JSON field names (config items and download state)                           */
/* -------------------------------------------------------------------------- */

#define AVAR_FIELD_ID "id"
#define AVAR_FIELD_URL "url"
#define AVAR_FIELD_STATUS "status"
#define AVAR_FIELD_FILENAME "filename"
#define AVAR_FIELD_FILENAME_INFERRED "filenameInferred"
#define AVAR_FIELD_MAX_RETRIES "maxRetries"
#define AVAR_FIELD_ERROR_COUNT "errorCount"
#define AVAR_FIELD_PROXY "proxy"
#define AVAR_FIELD_BYTES_DOWNLOADED "bytesDownloaded"
#define AVAR_FIELD_TOTAL_BYTES "totalBytes"
#define AVAR_FIELD_QUEUED_AT "queuedAt"
#define AVAR_FIELD_LAST_TRY_AT "lastTryAt"
#define AVAR_FIELD_DESCRIPTION "description"
#define AVAR_FIELD_ORIGINAL_PAGE "originalPage"
#define AVAR_FIELD_REFERER "referer"
#define AVAR_FIELD_STREAM_KIND "streamKind"
#define AVAR_FIELD_ADDED_THROUGH "addedThrough"
#define AVAR_FIELD_QUEUE_ID "queueId"

#define AVAR_QUEUE_FIELD_NAME "name"
#define AVAR_QUEUE_FIELD_MAX_CONCURRENT "maxConcurrentDownloads"
#define AVAR_QUEUE_FIELD_MAX_CONNECTIONS "maxConnections"
#define AVAR_QUEUE_FIELD_MAX_RETRIES "maxRetries"
#define AVAR_QUEUE_FIELD_TEMP_PATH "tempPath"
#define AVAR_QUEUE_FIELD_DOWNLOAD_PATH "downloadPath"
#define AVAR_QUEUE_FIELD_STARTED "started"
#define AVAR_FIELD_ETAG "etag"
#define AVAR_FIELD_LAST_MODIFIED "lastModified"
#define AVAR_FIELD_CHUNK_SIZE "chunkSize"
#define AVAR_FIELD_CHUNK_COUNT "chunkCount"
#define AVAR_FIELD_CHUNKS "chunks"

#define AVAR_STATE_FIELD_TEMP_PATH "temp_path"
#define AVAR_STATE_FIELD_DEST_PATH "dest_path"
#define AVAR_STATE_FIELD_CHUNK_SIZE "chunk_size"
#define AVAR_STATE_FIELD_CHUNK_COUNT "chunk_count"
#define AVAR_STATE_FIELD_DONE_RANGES "done_ranges"
#define AVAR_STATE_FIELD_TOTAL_SIZE "total_size"
#define AVAR_STATE_TMP_SUFFIX ".tmp"

/* -------------------------------------------------------------------------- */
/* Daemon                                                                     */
/* -------------------------------------------------------------------------- */

#define AVAR_DAEMON_PORT 8000
#define AVAR_DAEMON_POLL_MS 1000U
#define AVAR_DAEMON_HTTP_BIND_DEFAULT "0.0.0.0"
#define AVAR_DAEMON_PID_FILENAME "avar.pid"
#define AVAR_DAEMON_SYSTEMD_UNIT "avar-daemon.service"

#if defined(_WIN32)
#define AVAR_DAEMON_PIPE_NAME_WIN "\\\\.\\pipe\\avar"
#else
#define AVAR_DAEMON_PIPE_NAME_UNIX "/tmp/avar.pipe"
#endif

#define AVAR_CFG_DAEMON "daemon"
#define AVAR_CFG_DAEMON_SESSION "daemon.session"
#define AVAR_CFG_DAEMON_SESSION_MODE "daemon.session.mode"
#define AVAR_CFG_DAEMON_SESSION_TRANSPORT "daemon.session.transport"
#define AVAR_CFG_DAEMON_SESSION_REMOTE_HOST "daemon.session.remoteHost"
#define AVAR_CFG_DAEMON_SESSION_REMOTE_PORT "daemon.session.remotePort"
#define AVAR_CFG_DAEMON_SERVER "daemon.server"
#define AVAR_CFG_DAEMON_SERVER_DETACH "daemon.server.detach"
#define AVAR_CFG_DAEMON_SERVER_PID_FILE "daemon.server.pidFile"
#define AVAR_CFG_DAEMON_SERVER_CONTAINER "daemon.server.containerMode"
#define AVAR_CFG_DAEMON_SERVER_AUTH_TOKEN "daemon.server.authToken"
#define AVAR_CFG_DAEMON_SERVER_CORS_ENABLED "daemon.server.cors.enabled"
#define AVAR_CFG_DAEMON_SERVER_CORS_ALLOW_ORIGIN "daemon.server.cors.allowOrigin"
#define AVAR_CFG_DAEMON_SERVER_FILE_DOWNLOAD_ENABLED "daemon.server.fileDownload.enabled"
#define AVAR_CFG_DAEMON_SERVER_FS_BROWSE_ENABLED "daemon.server.fsBrowse.enabled"
#define AVAR_CFG_DAEMON_CHANNELS_HTTP "daemon.server.channels.http"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS "daemon.server.channels.https"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS_ENABLED "daemon.server.channels.https.enabled"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS_BIND "daemon.server.channels.https.bind"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS_PORT "daemon.server.channels.https.port"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS_CERT "daemon.server.channels.https.certFile"
#define AVAR_CFG_DAEMON_CHANNELS_HTTPS_KEY "daemon.server.channels.https.keyFile"
#define AVAR_CFG_DAEMON_CHANNELS_HTTP_ENABLED "daemon.server.channels.http.enabled"
#define AVAR_CFG_DAEMON_CHANNELS_HTTP_BIND "daemon.server.channels.http.bind"
#define AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT "daemon.server.channels.http.port"
#define AVAR_CFG_DAEMON_CHANNELS_PIPE "daemon.server.channels.pipe"
#define AVAR_CFG_DAEMON_CHANNELS_PIPE_ENABLED "daemon.server.channels.pipe.enabled"
#define AVAR_CFG_DAEMON_CHANNELS_PIPE_NAME "daemon.server.channels.pipe.name"
#define AVAR_CFG_DAEMON_CHANNELS_UNIX "daemon.server.channels.unix"
#define AVAR_CFG_DAEMON_CHANNELS_UNIX_ENABLED "daemon.server.channels.unix.enabled"
#define AVAR_CFG_DAEMON_CHANNELS_UNIX_PATH "daemon.server.channels.unix.path"
#define AVAR_CFG_DAEMON_SERVER_AUTO_SHUTDOWN "daemon.server.autoShutdown"
#define AVAR_CFG_DAEMON_SERVER_AUTO_SHUTDOWN_IDLE_SECONDS "daemon.server.autoShutdownIdleSeconds"

#define AVAR_DAEMON_AUTO_SHUTDOWN_NEVER "never"
#define AVAR_DAEMON_AUTO_SHUTDOWN_WHEN_IDLE "whenIdle"
#define AVAR_DAEMON_AUTO_SHUTDOWN_IDLE_SECONDS_DEFAULT 60U

#define AVAR_DAEMON_SESSION_MODE_LOCAL "local"
#define AVAR_DAEMON_SESSION_MODE_REMOTE "remote"
#define AVAR_DAEMON_TRANSPORT_LOCAL "local"
#define AVAR_DAEMON_TRANSPORT_PIPE "pipe"
#define AVAR_DAEMON_TRANSPORT_UNIX "unix"
#define AVAR_DAEMON_TRANSPORT_HTTP "http"

/* -------------------------------------------------------------------------- */
/* Buffers and limits                                                         */
/* -------------------------------------------------------------------------- */

#define AVAR_FS_COPY_BUFFER (64U * AVAR_KIB)
#define AVAR_HTTP_DECODE_BUFFER 1024U
#define AVAR_DATETIME_BUF_SIZE 32U
#define AVAR_DL_ERROR_BUF_SIZE 256U
#define AVAR_DL_ID_BUF_SIZE 64U
#define AVAR_DL_FALLBACK_NAME_BUF_SIZE 64U
#define AVAR_DAEMON_URL_BUF_SIZE 256U
#define AVAR_DAEMON_LAUNCHD_LABEL "com.avar.daemon"

/* -------------------------------------------------------------------------- */
/* CLI                                                                        */
/* -------------------------------------------------------------------------- */

#define AVAR_CLI_HELP_SHORT "-h"
#define AVAR_CLI_HELP_LONG "--help"

void fatal(const char *reason_or_null);

#endif
