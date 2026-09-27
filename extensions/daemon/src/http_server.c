#include "avar/http_server.h"

#include <stdint.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct avar_http_server {
    int listen_fd;
    bool running;
    avar_http_handler handler;
    void *user_data;
};

avar_http_server *avar_http_server_create(const char *host, int port, avar_http_handler handler, void *user_data)
{
    avar_http_server *server = calloc(1, sizeof(*server));
    if (server == NULL) {
        return NULL;
    }
    server->handler = handler;
    server->user_data = user_data;

    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        free(server);
        return NULL;
    }

    int yes = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(host != NULL ? host : "127.0.0.1");

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    if (listen(server->listen_fd, 16) < 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    return server;
}

static void handle_client(avar_http_server *server, int client_fd)
{
    char buffer[8192];
    const ssize_t read_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (read_bytes <= 0) {
        close(client_fd);
        return;
    }
    buffer[read_bytes] = '\0';

    avar_http_request request;
    memset(&request, 0, sizeof(request));

    char method[16];
    char path[512];
    if (sscanf(buffer, "%15s %511s", method, path) != 2) {
        avar_http_send_json(client_fd, 400, "{\"ok\":false,\"error\":\"Bad request\"}");
        close(client_fd);
        return;
    }
    request.method = method;
    request.path = path;

    const char *body = strstr(buffer, "\r\n\r\n");
    if (body != NULL) {
        body += 4;
        request.body = body;
        request.body_length = strlen(body);
    }

    server->handler(&request, client_fd, server->user_data);
    close(client_fd);
}

int avar_http_server_run(avar_http_server *server)
{
    if (server == NULL) {
        return -1;
    }
    server->running = true;
    while (server->running) {
        const int client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        handle_client(server, client_fd);
    }
    return 0;
}

void avar_http_server_stop(avar_http_server *server)
{
    if (server != NULL) {
        server->running = false;
    }
}

void avar_http_server_destroy(avar_http_server *server)
{
    if (server == NULL) {
        return;
    }
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
    free(server);
}

void avar_http_send_json(int client_fd, int status_code, const char *json_body)
{
    char header[256];
    const int header_len = snprintf(header,
                                    sizeof(header),
                                    "HTTP/1.1 %d OK\r\n"
                                    "Content-Type: application/json\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Connection: close\r\n"
                                    "Content-Length: %zu\r\n\r\n",
                                    status_code,
                                    json_body != NULL ? strlen(json_body) : 0);
    send(client_fd, header, (size_t)header_len, 0);
    if (json_body != NULL) {
        send(client_fd, json_body, strlen(json_body), 0);
    }
}
