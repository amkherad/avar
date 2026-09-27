#ifndef AVAR_HTTP_SERVER_H
#define AVAR_HTTP_SERVER_H

#include <stddef.h>

typedef struct avar_http_request {
    const char *method;
    const char *path;
    const char *body;
    size_t body_length;
} avar_http_request;

typedef void (*avar_http_handler)(const avar_http_request *request, int client_fd, void *user_data);

typedef struct avar_http_server avar_http_server;

avar_http_server *avar_http_server_create(const char *host, int port, avar_http_handler handler, void *user_data);

int avar_http_server_run(avar_http_server *server);

void avar_http_server_stop(avar_http_server *server);

void avar_http_server_destroy(avar_http_server *server);

void avar_http_send_json(int client_fd, int status_code, const char *json_body);

#endif
