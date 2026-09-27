#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "avar/daemon_rpc.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_url(const char *url, char *host, size_t host_size, char *path, size_t path_size, int *port)
{
    const char *start = url;
    if (strncmp(start, "http://", 7) == 0) {
        start += 7;
        *port = 80;
    } else if (strncmp(start, "https://", 8) == 0) {
        start += 8;
        *port = 443;
    } else {
        return -1;
    }

    const char *slash = strchr(start, '/');
    const char *host_end = slash != NULL ? slash : start + strlen(start);
    const char *colon = memchr(start, ':', (size_t)(host_end - start));
    size_t host_len;
    if (colon != NULL && colon < host_end) {
        host_len = (size_t)(colon - start);
        *port = atoi(colon + 1);
    } else {
        host_len = (size_t)(host_end - start);
    }
    if (host_len >= host_size) {
        host_len = host_size - 1;
    }
    memcpy(host, start, host_len);
    host[host_len] = '\0';

    if (slash != NULL) {
        snprintf(path, path_size, "%s", slash);
    } else {
        snprintf(path, path_size, "/");
    }
    return 0;
}

int avar_daemon_rpc_call(const avar_daemon_rpc_config *cfg,
                         const char *method,
                         const char *params_json,
                         char *out,
                         size_t out_size)
{
    if (cfg == NULL || method == NULL || out == NULL || out_size == 0) {
        return -1;
    }

    char host[256];
    char path[512];
    int port = 80;
    if (parse_url(cfg->daemon_url, host, sizeof(host), path, sizeof(path), &port) != 0) {
        return -1;
    }

    const char *rpc_path = "/api/rpc";

    char body[4096];
    snprintf(body,
             sizeof(body),
             "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}",
             method,
             params_json != NULL ? params_json : "{}");

    char request[4800];
    int req_len = snprintf(request,
                           sizeof(request),
                           "POST %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Connection: close\r\n",
                           rpc_path,
                           host);
    if (cfg->auth_token[0] != '\0') {
        req_len += snprintf(request + req_len,
                            sizeof(request) - (size_t)req_len,
                            "Authorization: Bearer %s\r\n",
                            cfg->auth_token);
    }
    req_len += snprintf(request + req_len,
                        sizeof(request) - (size_t)req_len,
                        "Content-Length: %zu\r\n\r\n%s",
                        strlen(body),
                        body);

    struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    struct addrinfo *res = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == NULL) {
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (send(fd, request, (size_t)req_len, 0) < 0) {
        close(fd);
        return -1;
    }

    out[0] = '\0';
    char chunk[1024];
    ssize_t n;
    size_t total = 0;
    while ((n = recv(fd, chunk, sizeof(chunk) - 1, 0)) > 0) {
        chunk[n] = '\0';
        if (total + (size_t)n >= out_size) {
            n = (ssize_t)(out_size - total - 1);
        }
        memcpy(out + total, chunk, (size_t)n);
        total += (size_t)n;
        out[total] = '\0';
        if (total >= out_size - 1) {
            break;
        }
    }
    close(fd);

    const char *json = strstr(out, "\r\n\r\n");
    if (json != NULL) {
        json += 4;
        memmove(out, json, strlen(json) + 1);
    }
    return 0;
}
