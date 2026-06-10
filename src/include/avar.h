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
#ifndef VERSION
#define VERSION 0.0.1
#endif
#ifndef VERSION_STR
#define VERSION_STR TOSTR(VERSION)
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
#define AVAR_DEFAULT_SPEED_UNIT AVAR_UNIT_MIBIT_PER_SEC

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

/* -------------------------------------------------------------------------- */
/* Config keys                                                                */
/* -------------------------------------------------------------------------- */

#define AVAR_CFG_DM_ITEMS "dm.items"
#define AVAR_CFG_DM_TEMP_PATH "dm.tempPath"
#define AVAR_CFG_DM_DOWNLOAD_PATH "dm.downloadPath"
#define AVAR_CFG_DM_PROGRESS_SIZE_UNIT "dm.progress.sizeUnit"
#define AVAR_CFG_DM_PROGRESS_SPEED_UNIT "dm.progress.speedUnit"

/* -------------------------------------------------------------------------- */
/* Download manager                                                           */
/* -------------------------------------------------------------------------- */

#define DL_STATE_FILENAME "state.json"
#define DL_CHUNK_SIZE (256U * AVAR_KIB)
#define DL_CONNECT_TIMEOUT_MS 30000U
#define DL_IDLE_TIMEOUT_MS 120000U
#define DL_MAX_REDIRECTS 10
#define DL_WRITE_CHUNK_SIZE AVAR_MIB
#define DL_PROGRESS_BAR_WIDTH 22
#define DL_PROGRESS_PERCENT_MAX 100
#define DL_POLL_MS 50U

#define AVAR_DL_ID_PREFIX "dl-"
#define AVAR_DOWNLOAD_FALLBACK_PREFIX "download-"
#define AVAR_DOWNLOAD_FALLBACK_SUFFIX ".bin"

#define AVAR_DL_STATUS_COMPLETED "completed"
#define AVAR_DL_STATUS_DOWNLOADING "downloading"
#define AVAR_DL_STATUS_FAILED "failed"
#define AVAR_DL_STATUS_QUEUED "queued"

#define AVAR_DL_ADDED_DIRECT "direct"

/* -------------------------------------------------------------------------- */
/* JSON field names (config items and download state)                           */
/* -------------------------------------------------------------------------- */

#define AVAR_FIELD_ID "id"
#define AVAR_FIELD_URL "url"
#define AVAR_FIELD_STATUS "status"
#define AVAR_FIELD_FILENAME "filename"
#define AVAR_FIELD_PROXY "proxy"
#define AVAR_FIELD_BYTES_DOWNLOADED "bytesDownloaded"
#define AVAR_FIELD_TOTAL_BYTES "totalBytes"
#define AVAR_FIELD_QUEUED_AT "queuedAt"
#define AVAR_FIELD_LAST_TRY_AT "lastTryAt"
#define AVAR_FIELD_DESCRIPTION "description"
#define AVAR_FIELD_ORIGINAL_PAGE "originalPage"
#define AVAR_FIELD_REFERER "referer"
#define AVAR_FIELD_ADDED_THROUGH "addedThrough"
#define AVAR_FIELD_QUEUE_ID "queueId"
#define AVAR_FIELD_ETAG "etag"
#define AVAR_FIELD_LAST_MODIFIED "lastModified"
#define AVAR_FIELD_CHUNK_SIZE "chunkSize"
#define AVAR_FIELD_CHUNK_COUNT "chunkCount"
#define AVAR_FIELD_CHUNKS "chunks"

#define AVAR_STATE_FIELD_TEMP_PATH "temp_path"
#define AVAR_STATE_FIELD_DEST_PATH "dest_path"
#define AVAR_STATE_FIELD_CHUNK_SIZE "chunk_size"
#define AVAR_STATE_FIELD_CHUNK_COUNT "chunk_count"
#define AVAR_STATE_FIELD_TOTAL_SIZE "total_size"
#define AVAR_STATE_TMP_SUFFIX ".tmp"

/* -------------------------------------------------------------------------- */
/* Daemon                                                                     */
/* -------------------------------------------------------------------------- */

#define AVAR_DAEMON_PORT 8000
#define AVAR_DAEMON_POLL_MS 1000U

/* -------------------------------------------------------------------------- */
/* Buffers and limits                                                         */
/* -------------------------------------------------------------------------- */

#define AVAR_FS_COPY_BUFFER (64U * AVAR_KIB)
#define AVAR_HTTP_DECODE_BUFFER 1024U
#define AVAR_DATETIME_BUF_SIZE 32U
#define AVAR_DL_ERROR_BUF_SIZE 256U
#define AVAR_DL_ID_BUF_SIZE 64U
#define AVAR_DL_FALLBACK_NAME_BUF_SIZE 64U
#define AVAR_DAEMON_URL_BUF_SIZE 32U

/* -------------------------------------------------------------------------- */
/* CLI                                                                        */
/* -------------------------------------------------------------------------- */

#define AVAR_CLI_HELP_SHORT "-h"
#define AVAR_CLI_HELP_LONG "--help"

void fatal(const char *reason_or_null);

#endif
