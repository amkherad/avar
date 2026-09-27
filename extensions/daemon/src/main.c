#include "avar/bridge_handlers.h"
#include "avar/extension_protocol.h"
#include "avar/http_server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static avar_http_server *g_server = NULL;

static void on_signal(int sig)
{
    (void)sig;
    if (g_server != NULL) {
        avar_http_server_stop(g_server);
    }
}

static void dispatch(const avar_http_request *request, int client_fd, void *user_data)
{
    avar_bridge_handle_request(request, client_fd, (avar_bridge_state *)user_data);
}

int main(int argc, char **argv)
{
    int port = AVAR_EXTENSION_DEFAULT_PORT;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    avar_bridge_state state;
    memset(&state, 0, sizeof(state));
    state.settings.enabled = true;
    snprintf(state.settings.daemon_url, sizeof(state.settings.daemon_url), "%s", "http://127.0.0.1:8000");

    g_server = avar_http_server_create("127.0.0.1", port, dispatch, &state);
    if (g_server == NULL) {
        fprintf(stderr, "avar-extension-daemon: failed to bind 127.0.0.1:%d\n", port);
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    fprintf(stderr, "avar-extension-daemon listening on http://127.0.0.1:%d\n", port);
    const int rc = avar_http_server_run(g_server);
    avar_http_server_destroy(g_server);
    return rc == 0 ? 0 : 1;
}
