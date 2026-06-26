#include "file_checksum.h"

#include "file-system.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/md.h>
#include <mongoose.h>

#define FILE_CHECKSUM_BUF_SIZE (64U * 1024U)

static char *bytes_to_lower_hex(const unsigned char *data, size_t len) {
    if (data == NULL || len == 0U) {
        return NULL;
    }

    char *out = malloc(len * 2U + 1U);
    if (out == NULL) {
        return NULL;
    }

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2U] = hex[(data[i] >> 4) & 0x0f];
        out[i * 2U + 1U] = hex[data[i] & 0x0f];
    }
    out[len * 2U] = '\0';
    return out;
}

static mbedtls_md_type_t mbedtls_type_for_algorithm(const char *algorithm) {
    if (algorithm == NULL) {
        return MBEDTLS_MD_NONE;
    }
    if (strcmp(algorithm, "md5") == 0) {
        return MBEDTLS_MD_MD5;
    }
    if (strcmp(algorithm, "sha1") == 0) {
        return MBEDTLS_MD_SHA1;
    }
    if (strcmp(algorithm, "sha224") == 0) {
        return MBEDTLS_MD_SHA224;
    }
    if (strcmp(algorithm, "sha256") == 0) {
        return MBEDTLS_MD_SHA256;
    }
    if (strcmp(algorithm, "sha384") == 0) {
        return MBEDTLS_MD_SHA384;
    }
    if (strcmp(algorithm, "sha512") == 0) {
        return MBEDTLS_MD_SHA512;
    }
    if (strcmp(algorithm, "ripemd160") == 0) {
        return MBEDTLS_MD_RIPEMD160;
    }
    return MBEDTLS_MD_NONE;
}

bool file_checksum_algorithm_supported(const char *algorithm) {
    if (algorithm == NULL) {
        return false;
    }
    if (strcmp(algorithm, "crc32") == 0) {
        return true;
    }
    return mbedtls_type_for_algorithm(algorithm) != MBEDTLS_MD_NONE;
}

static int file_crc32_hex(const char *path, char **hex_out) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t crc = 0;
    unsigned char buffer[FILE_CHECKSUM_BUF_SIZE];
    for (;;) {
        const size_t read = fread(buffer, 1U, sizeof buffer, fp);
        if (read == 0U) {
            if (ferror(fp) != 0) {
                fclose(fp);
                return EXIT_FAILURE;
            }
            break;
        }
        crc = mg_crc32(crc, (const char *)buffer, read);
    }
    fclose(fp);

    const uint32_t value = crc;
    unsigned char digest[4] = {
        (unsigned char)((value >> 24) & 0xffU),
        (unsigned char)((value >> 16) & 0xffU),
        (unsigned char)((value >> 8) & 0xffU),
        (unsigned char)(value & 0xffU),
    };

    *hex_out = bytes_to_lower_hex(digest, sizeof digest);
    return *hex_out != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int file_mbedtls_hex(const char *path, mbedtls_md_type_t md_type, char **hex_out) {
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(md_type);
    if (info == NULL) {
        return EXIT_FAILURE;
    }

    unsigned char digest[MBEDTLS_MD_MAX_SIZE];
    const int rc = mbedtls_md_file(info, path, digest);
    if (rc != 0) {
        return EXIT_FAILURE;
    }

    const size_t digest_len = mbedtls_md_get_size(info);
    *hex_out = bytes_to_lower_hex(digest, digest_len);
    return *hex_out != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}

int file_checksum_file(const char *path, const char *algorithm, char **hex_out) {
    if (path == NULL || algorithm == NULL || hex_out == NULL) {
        return EXIT_FAILURE;
    }
    *hex_out = NULL;

    if (!file_exists(path)) {
        return EXIT_FAILURE;
    }

    if (strcmp(algorithm, "crc32") == 0) {
        return file_crc32_hex(path, hex_out);
    }

    const mbedtls_md_type_t md_type = mbedtls_type_for_algorithm(algorithm);
    if (md_type == MBEDTLS_MD_NONE) {
        return EXIT_FAILURE;
    }

    return file_mbedtls_hex(path, md_type, hex_out);
}

void file_checksum_normalize(const char *value, char *out, size_t out_len) {
    if (out == NULL || out_len == 0U) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    const char *text = value;
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    const char *colon = strchr(text, ':');
    if (colon != NULL && colon > text && (size_t)(colon - text) < 16U) {
        bool label = true;
        for (const char *p = text; p < colon; ++p) {
            if (!isalnum((unsigned char)*p)) {
                label = false;
                break;
            }
        }
        if (label) {
            text = colon + 1;
            while (*text != '\0' && isspace((unsigned char)*text)) {
                ++text;
            }
        }
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    size_t pos = 0U;
    for (const char *p = text; *p != '\0' && pos + 1U < out_len; ++p) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '-') {
            continue;
        }
        out[pos++] = (char)tolower((unsigned char)*p);
    }
    out[pos] = '\0';
}

bool file_checksum_matches(const char *computed, const char *expected) {
    char left[256];
    char right[256];
    file_checksum_normalize(computed, left, sizeof left);
    file_checksum_normalize(expected, right, sizeof right);
    if (left[0] == '\0' || right[0] == '\0') {
        return false;
    }
    return strcmp(left, right) == 0;
}
