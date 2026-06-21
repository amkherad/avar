#include <cli/electron_embed.h>

#include <avar.h>
#include <logger.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <limits.h>
    #include <sys/stat.h>
    #include <unistd.h>

    #if defined(__APPLE__)
        #include <mach-o/dyld.h>
    #endif
#endif

#define ELECTRON_EMBED_MAGIC "AVAREMB1"
#define ELECTRON_EMBED_DIGEST_LEN 64U
#define ELECTRON_EMBED_EXE_REL_LEN 256U
#define ELECTRON_EMBED_TRAILER_LEN 336U

typedef struct {
    uint64_t archive_size;
    char digest[ELECTRON_EMBED_DIGEST_LEN + 1U];
    char exe_rel[ELECTRON_EMBED_EXE_REL_LEN];
} ElectronEmbedTrailer;

static int resolve_self_path(char *buf, size_t buflen);
static bool read_embed_trailer(ElectronEmbedTrailer *out);
static bool extract_archive_to_cache(const ElectronEmbedTrailer *trailer, char *cache_dir, size_t cache_dir_len);
static bool run_extract_command(const char *archive_path, const char *dest_dir);

static int resolve_self_path(char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return -1;
    }

#if defined(_WIN32)
    const DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)buflen);
    if (n == 0 || n >= buflen) {
        return -1;
    }
    return 0;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)buflen;
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return -1;
    }
    return 0;
#else
    const ssize_t n = readlink("/proc/self/exe", buf, buflen - 1U);
    if (n <= 0) {
        return -1;
    }
    buf[(size_t)n] = '\0';
    return 0;
#endif
}

static bool read_embed_trailer(ElectronEmbedTrailer *out) {
    if (out == NULL) {
        return false;
    }

    char self_path[AVAR_CONFIG_PATH_MAX];
    if (resolve_self_path(self_path, sizeof self_path) != 0) {
        return false;
    }

    FILE *fh = fopen(self_path, "rb");
    if (fh == NULL) {
        return false;
    }

    if (fseek(fh, 0, SEEK_END) != 0) {
        fclose(fh);
        return false;
    }

    long file_size = ftell(fh);
    if (file_size < 0 || (unsigned long)file_size < ELECTRON_EMBED_TRAILER_LEN) {
        fclose(fh);
        return false;
    }

    const long trailer_pos = file_size - (long)ELECTRON_EMBED_TRAILER_LEN;
    if (fseek(fh, trailer_pos, SEEK_SET) != 0) {
        fclose(fh);
        return false;
    }

    unsigned char trailer_raw[ELECTRON_EMBED_TRAILER_LEN];
    if (fread(trailer_raw, 1, sizeof trailer_raw, fh) != sizeof trailer_raw) {
        fclose(fh);
        return false;
    }
    fclose(fh);

    if (memcmp(trailer_raw, ELECTRON_EMBED_MAGIC, 8) != 0) {
        return false;
    }

    uint64_t archive_size = 0;
    memcpy(&archive_size, trailer_raw + 8, sizeof archive_size);

    if (archive_size == 0U ||
        archive_size > (uint64_t)(file_size - (long)ELECTRON_EMBED_TRAILER_LEN)) {
        return false;
    }

    memset(out, 0, sizeof *out);
    out->archive_size = archive_size;
    memcpy(out->digest, trailer_raw + 16, ELECTRON_EMBED_DIGEST_LEN);
    out->digest[ELECTRON_EMBED_DIGEST_LEN] = '\0';

    size_t exe_rel_len = 0;
    while (exe_rel_len < ELECTRON_EMBED_EXE_REL_LEN && trailer_raw[80U + exe_rel_len] != '\0') {
        out->exe_rel[exe_rel_len] = (char)trailer_raw[80U + exe_rel_len];
        ++exe_rel_len;
    }
    out->exe_rel[exe_rel_len] = '\0';

    return out->exe_rel[0] != '\0';
}

static bool run_extract_command(const char *archive_path, const char *dest_dir) {
    if (archive_path == NULL || dest_dir == NULL) {
        return false;
    }

    char cmd[AVAR_CONFIG_PATH_MAX * 3];
#if defined(_WIN32)
    snprintf(cmd, sizeof cmd, "tar -xf \"%s\" -C \"%s\"", archive_path, dest_dir);
#else
    snprintf(cmd, sizeof cmd, "tar -xf '%s' -C '%s'", archive_path, dest_dir);
#endif

    LOG_DEBUG("Extracting embedded Electron: %s", cmd);
    return system(cmd) == 0;
}

static bool write_archive_slice(const ElectronEmbedTrailer *trailer, const char *archive_path) {
    char self_path[AVAR_CONFIG_PATH_MAX];
    if (resolve_self_path(self_path, sizeof self_path) != 0) {
        return false;
    }

    FILE *in = fopen(self_path, "rb");
    if (in == NULL) {
        return false;
    }

    if (fseek(in, 0, SEEK_END) != 0) {
        fclose(in);
        return false;
    }

    const long file_size = ftell(in);
    if (file_size < 0) {
        fclose(in);
        return false;
    }

    const long archive_pos = file_size - (long)ELECTRON_EMBED_TRAILER_LEN - (long)trailer->archive_size;
    if (archive_pos < 0) {
        fclose(in);
        return false;
    }

    if (fseek(in, archive_pos, SEEK_SET) != 0) {
        fclose(in);
        return false;
    }

    FILE *out = fopen(archive_path, "wb");
    if (out == NULL) {
        fclose(in);
        return false;
    }

    unsigned char buffer[64U * 1024U];
    uint64_t remaining = trailer->archive_size;
    while (remaining > 0U) {
        const size_t chunk = remaining > sizeof buffer ? sizeof buffer : (size_t)remaining;
        const size_t read_bytes = fread(buffer, 1, chunk, in);
        if (read_bytes == 0) {
            fclose(in);
            fclose(out);
            remove(archive_path);
            return false;
        }
        if (fwrite(buffer, 1, read_bytes, out) != read_bytes) {
            fclose(in);
            fclose(out);
            remove(archive_path);
            return false;
        }
        remaining -= (uint64_t)read_bytes;
    }

    fclose(in);
    fclose(out);
    return true;
}

static bool extract_archive_to_cache(const ElectronEmbedTrailer *trailer, char *cache_dir,
                                     const size_t cache_dir_len) {
    if (trailer == NULL || cache_dir == NULL || cache_dir_len == 0) {
        return false;
    }

    const char *base = getenv("AVAR_ELECTRON_CACHE");
    if (base == NULL || base[0] == '\0') {
#if defined(_WIN32)
        base = getenv("LOCALAPPDATA");
        if (base == NULL || base[0] == '\0') {
            base = getenv("TEMP");
        }
#else
        base = "/tmp";
#endif
    }

    snprintf(cache_dir, cache_dir_len, "%s/avar-electron/%s", base, trailer->digest);

    char marker_path[AVAR_CONFIG_PATH_MAX];
    snprintf(marker_path, sizeof marker_path, "%s/.extracted", cache_dir);

    FILE *marker = fopen(marker_path, "rb");
    if (marker != NULL) {
        char marker_digest[ELECTRON_EMBED_DIGEST_LEN + 1U] = {0};
        (void)fread(marker_digest, 1, ELECTRON_EMBED_DIGEST_LEN, marker);
        fclose(marker);
        if (strncmp(marker_digest, trailer->digest, ELECTRON_EMBED_DIGEST_LEN) == 0) {
            return true;
        }
    }

#if defined(_WIN32)
    char parent_path[AVAR_CONFIG_PATH_MAX];
    snprintf(parent_path, sizeof parent_path, "%s\\avar-electron", base);
    CreateDirectoryA(parent_path, NULL);
    CreateDirectoryA(cache_dir, NULL);
#else
    char parent_path[AVAR_CONFIG_PATH_MAX];
    snprintf(parent_path, sizeof parent_path, "%s/avar-electron", base);
    (void)mkdir(parent_path, 0755);
    (void)mkdir(cache_dir, 0755);
#endif

    char archive_path[AVAR_CONFIG_PATH_MAX];
    snprintf(archive_path, sizeof archive_path, "%s/bundle.tar", cache_dir);

    if (!write_archive_slice(trailer, archive_path)) {
        return false;
    }

    if (!run_extract_command(archive_path, cache_dir)) {
        remove(archive_path);
        return false;
    }

    remove(archive_path);

    marker = fopen(marker_path, "wb");
    if (marker != NULL) {
        (void)fwrite(trailer->digest, 1, strlen(trailer->digest), marker);
        fclose(marker);
    }

    return true;
}

bool electron_embed_available(void) {
    ElectronEmbedTrailer trailer;
    return read_embed_trailer(&trailer);
}

bool electron_embed_resolve_exe(char *exe_path, const size_t exe_path_len) {
    if (exe_path == NULL || exe_path_len == 0) {
        return false;
    }

    ElectronEmbedTrailer trailer;
    if (!read_embed_trailer(&trailer)) {
        return false;
    }

    char cache_dir[AVAR_CONFIG_PATH_MAX];
    if (!extract_archive_to_cache(&trailer, cache_dir, sizeof cache_dir)) {
        LOG_ERROR("Failed to extract embedded Electron bundle");
        return false;
    }

#if defined(_WIN32)
    snprintf(exe_path, exe_path_len, "%s\\%s", cache_dir, trailer.exe_rel);
    for (char *p = exe_path; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\\';
        }
    }
#else
    snprintf(exe_path, exe_path_len, "%s/%s", cache_dir, trailer.exe_rel);
#endif

#if defined(_WIN32)
    if (GetFileAttributesA(exe_path) == INVALID_FILE_ATTRIBUTES) {
#else
    if (access(exe_path, X_OK) != 0) {
#endif
        LOG_ERROR("Embedded Electron executable not found at %s", exe_path);
        return false;
    }

    LOG_INFO("Using embedded Electron at %s", exe_path);
    return true;
}
