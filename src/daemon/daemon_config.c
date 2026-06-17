#include <config.h>
#include <daemon/daemon.h>
#include <file-system.h>
#include <instance.h>
#include <logger.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_session_mode(stringa text, AvarSessionMode *out) {
    if (text == NULL || out == NULL) {
        return false;
    }
    if (strcmp(text, AVAR_DAEMON_SESSION_MODE_LOCAL) == 0) {
        *out = AvarSessionModeLocal;
        return true;
    }
    if (strcmp(text, AVAR_DAEMON_SESSION_MODE_REMOTE) == 0) {
        *out = AvarSessionModeRemote;
        return true;
    }
    return false;
}

static bool parse_transport_kind(stringa text, AvarTransportKind *out) {
    if (text == NULL || out == NULL) {
        return false;
    }
    if (strcmp(text, AVAR_DAEMON_TRANSPORT_LOCAL) == 0) {
        *out = AvarTransportLocal;
        return true;
    }
    if (strcmp(text, AVAR_DAEMON_TRANSPORT_PIPE) == 0) {
        *out = AvarTransportPipe;
        return true;
    }
    if (strcmp(text, AVAR_DAEMON_TRANSPORT_UNIX) == 0) {
        *out = AvarTransportUnix;
        return true;
    }
    if (strcmp(text, AVAR_DAEMON_TRANSPORT_HTTP) == 0) {
        *out = AvarTransportHttp;
        return true;
    }
    return false;
}

static void default_pid_file_path(char *buf, size_t buflen) {
    char *dir = config_path(APP_ID);
    if (dir == NULL) {
        snprintf(buf, buflen, "avar.pid");
        return;
    }
    snprintf(buf, buflen, "%s%c%s", dir, PATH_SEPARATOR, AVAR_DAEMON_PID_FILENAME);
    free(dir);
}

#if defined(_WIN32)
static void default_pipe_name_win(char *buf, size_t buflen) {
    const char *user = getenv("USERNAME");
    if (user == NULL || user[0] == '\0') {
        const char *profile = getenv("USERPROFILE");
        if (profile != NULL && profile[0] != '\0') {
            const char *slash = strrchr(profile, '\\');
            if (slash != NULL && slash[1] != '\0') {
                user = slash + 1;
            }
        }
    }

    if (user != NULL && user[0] != '\0') {
        snprintf(buf, buflen, "\\\\.\\pipe\\avar-%s", user);
        return;
    }

    snprintf(buf, buflen, "%s", AVAR_DAEMON_PIPE_NAME_WIN);
}
#endif

void daemon_config_apply_defaults(DaemonConfig *cfg) {
    if (cfg == NULL) {
        return;
    }

    cfg->session.mode = AvarSessionModeLocal;
    cfg->session.transport = AvarTransportPipe;
    cfg->session.remote_host[0] = '\0';
    cfg->session.remote_port = AVAR_DAEMON_PORT;

    cfg->server.detach = true;
    cfg->server.container_mode = false;
    cfg->server.auth_token[0] = '\0';
    cfg->server.cors.enabled = true;
    snprintf(cfg->server.cors.allow_origin, sizeof cfg->server.cors.allow_origin, "%s", "*");
    default_pid_file_path(cfg->server.pid_file, sizeof cfg->server.pid_file);

    cfg->server.http.enabled = true;
    snprintf(cfg->server.http.bind_addr, sizeof cfg->server.http.bind_addr, "%s",
             AVAR_DAEMON_HTTP_BIND_DEFAULT);
    cfg->server.http.port = AVAR_DAEMON_PORT;

    cfg->server.https.enabled = false;
    snprintf(cfg->server.https.bind_addr, sizeof cfg->server.https.bind_addr, "%s",
             AVAR_DAEMON_HTTP_BIND_DEFAULT);
    cfg->server.https.port = AVAR_DAEMON_PORT + 1U;
    cfg->server.https.cert_file[0] = '\0';
    cfg->server.https.key_file[0] = '\0';

    cfg->server.pipe.enabled = true;
#if defined(_WIN32)
    default_pipe_name_win(cfg->server.pipe.name, sizeof cfg->server.pipe.name);
#else
    snprintf(cfg->server.pipe.name, sizeof cfg->server.pipe.name, "%s", AVAR_DAEMON_PIPE_NAME_UNIX);
#endif

    cfg->server.unix_socket.enabled = false;
    snprintf(cfg->server.unix_socket.path, sizeof cfg->server.unix_socket.path, "%s",
             AVAR_SOCK_DEFAULT);

    if (avar_instance_configured()) {
        avar_path_with_instance(cfg->server.pid_file, sizeof cfg->server.pid_file,
                                cfg->server.pid_file);
        avar_named_resource_with_instance(cfg->server.pipe.name, sizeof cfg->server.pipe.name,
                                          cfg->server.pipe.name);
        avar_path_with_instance(cfg->server.unix_socket.path, sizeof cfg->server.unix_socket.path,
                                cfg->server.unix_socket.path);

        const uint16_t offset = avar_instance_port_offset();
        cfg->server.http.port = (uint16_t)(cfg->server.http.port + offset);
        cfg->server.https.port = (uint16_t)(cfg->server.https.port + offset);
        cfg->session.remote_port = (uint16_t)(cfg->session.remote_port + offset);
    }
}

static void load_bool_key(stringa key, bool *out) {
    char *value = get_config(key);
    if (value == NULL) {
        return;
    }
    *out = strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
    free(value);
}

static void load_string_key(stringa key, char *out, size_t out_size) {
    char *value = get_config(key);
    if (value == NULL || out_size == 0) {
        free(value);
        return;
    }
    snprintf(out, out_size, "%s", value);
    free(value);
}

static void load_uint16_key(stringa key, uint16_t *out) {
    char *value = get_config(key);
    if (value == NULL) {
        return;
    }
    const long parsed = strtol(value, NULL, 10);
    if (parsed > 0 && parsed <= 65535) {
        *out = (uint16_t)parsed;
    }
    free(value);
}

bool daemon_config_load(DaemonConfig *out) {
    if (out == NULL) {
        return false;
    }

    daemon_config_apply_defaults(out);

    char *mode = get_config(AVAR_CFG_DAEMON_SESSION_MODE);
    if (mode != NULL) {
        AvarSessionMode parsed = AvarSessionModeLocal;
        if (!parse_session_mode(mode, &parsed)) {
            LOG_WARNING("Unknown daemon.session.mode '%s', using default", mode);
        } else {
            out->session.mode = parsed;
        }
        free(mode);
    }

    char *transport = get_config(AVAR_CFG_DAEMON_SESSION_TRANSPORT);
    if (transport != NULL) {
        AvarTransportKind parsed = AvarTransportPipe;
        if (!parse_transport_kind(transport, &parsed)) {
            LOG_WARNING("Unknown daemon.session.transport '%s', using default", transport);
        } else {
            out->session.transport = parsed;
        }
        free(transport);
    }

    load_string_key(AVAR_CFG_DAEMON_SESSION_REMOTE_HOST, out->session.remote_host,
                    sizeof out->session.remote_host);
    load_uint16_key(AVAR_CFG_DAEMON_SESSION_REMOTE_PORT, &out->session.remote_port);

    load_bool_key(AVAR_CFG_DAEMON_SERVER_DETACH, &out->server.detach);
    load_bool_key(AVAR_CFG_DAEMON_SERVER_CONTAINER, &out->server.container_mode);
    load_string_key(AVAR_CFG_DAEMON_SERVER_PID_FILE, out->server.pid_file,
                    sizeof out->server.pid_file);
    load_string_key(AVAR_CFG_DAEMON_SERVER_AUTH_TOKEN, out->server.auth_token,
                    sizeof out->server.auth_token);

    load_bool_key(AVAR_CFG_DAEMON_SERVER_CORS_ENABLED, &out->server.cors.enabled);
    load_string_key(AVAR_CFG_DAEMON_SERVER_CORS_ALLOW_ORIGIN, out->server.cors.allow_origin,
                    sizeof out->server.cors.allow_origin);

    load_bool_key(AVAR_CFG_DAEMON_CHANNELS_HTTP_ENABLED, &out->server.http.enabled);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_HTTP_BIND, out->server.http.bind_addr,
                    sizeof out->server.http.bind_addr);
    load_uint16_key(AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT, &out->server.http.port);

    load_bool_key(AVAR_CFG_DAEMON_CHANNELS_HTTPS_ENABLED, &out->server.https.enabled);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_HTTPS_BIND, out->server.https.bind_addr,
                    sizeof out->server.https.bind_addr);
    load_uint16_key(AVAR_CFG_DAEMON_CHANNELS_HTTPS_PORT, &out->server.https.port);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_HTTPS_CERT, out->server.https.cert_file,
                    sizeof out->server.https.cert_file);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_HTTPS_KEY, out->server.https.key_file,
                    sizeof out->server.https.key_file);

    load_bool_key(AVAR_CFG_DAEMON_CHANNELS_PIPE_ENABLED, &out->server.pipe.enabled);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_PIPE_NAME, out->server.pipe.name,
                    sizeof out->server.pipe.name);

    load_bool_key(AVAR_CFG_DAEMON_CHANNELS_UNIX_ENABLED, &out->server.unix_socket.enabled);
    load_string_key(AVAR_CFG_DAEMON_CHANNELS_UNIX_PATH, out->server.unix_socket.path,
                    sizeof out->server.unix_socket.path);

    return true;
}

void daemon_config_apply_start_options(DaemonConfig *cfg, const DaemonStartOptions *opts) {
    if (cfg == NULL || opts == NULL) {
        return;
    }

    if (opts->foreground || opts->no_detach) {
        cfg->server.detach = false;
    }
    if (opts->container_mode) {
        cfg->server.container_mode = true;
    }

    if (opts->http_override) {
        cfg->server.http.enabled = true;
    }
    if (opts->pipe_override) {
        cfg->server.pipe.enabled = true;
    }
    if (opts->unix_override) {
        cfg->server.unix_socket.enabled = true;
    }

    if (opts->http_port != NULL && opts->http_port[0] != '\0') {
        const long parsed = strtol(opts->http_port, NULL, 10);
        if (parsed > 0 && parsed <= 65535) {
            cfg->server.http.port = (uint16_t)parsed;
        }
    }

    if (opts->pipe_name != NULL && opts->pipe_name[0] != '\0') {
        snprintf(cfg->server.pipe.name, sizeof cfg->server.pipe.name, "%s", opts->pipe_name);
    }

    if (opts->unix_path != NULL && opts->unix_path[0] != '\0') {
        snprintf(cfg->server.unix_socket.path, sizeof cfg->server.unix_socket.path, "%s",
                 opts->unix_path);
        cfg->server.unix_socket.enabled = true;
    }

    if (opts->pid_file != NULL && opts->pid_file[0] != '\0') {
        snprintf(cfg->server.pid_file, sizeof cfg->server.pid_file, "%s", opts->pid_file);
    }
}
