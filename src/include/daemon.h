#ifndef AVAR_DAEMON_H
#define AVAR_DAEMON_H

#include <stdbool.h>
#include <stdint.h>

#include "avar.h"

/* -------------------------------------------------------------------------- */
/* Session and transport kinds (config string values in avar.h)               */
/* -------------------------------------------------------------------------- */

typedef enum {
    AvarSessionModeLocal,
    AvarSessionModeRemote,
} AvarSessionMode;

typedef enum {
    AvarTransportLocal,
    AvarTransportPipe,
    AvarTransportUnix,
    AvarTransportHttp,
} AvarTransportKind;

/* -------------------------------------------------------------------------- */
/* Channel configuration                                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    bool enabled;
    char bind_addr[64];
    uint16_t port;
} DaemonHttpChannel;

typedef struct {
    bool enabled;
    char bind_addr[64];
    uint16_t port;
    char cert_file[AVAR_CONFIG_PATH_MAX];
    char key_file[AVAR_CONFIG_PATH_MAX];
} DaemonHttpsChannel;

typedef struct {
    bool enabled;
    char name[AVAR_CONFIG_PATH_MAX];
} DaemonPipeChannel;

typedef struct {
    bool enabled;
    char path[AVAR_CONFIG_PATH_MAX];
} DaemonUnixChannel;

typedef struct {
    AvarSessionMode mode;
    AvarTransportKind transport;
    char remote_host[256];
    uint16_t remote_port;
} DaemonSessionConfig;

typedef struct {
    bool detach;
    bool container_mode;
    char pid_file[AVAR_CONFIG_PATH_MAX];
    char auth_token[128];
    DaemonHttpChannel http;
    DaemonHttpsChannel https;
    DaemonPipeChannel pipe;
    DaemonUnixChannel unix_socket;
} DaemonServerConfig;

typedef struct {
    DaemonSessionConfig session;
    DaemonServerConfig server;
} DaemonConfig;

typedef struct {
    bool foreground;
    bool http_override;
    bool pipe_override;
    bool unix_override;
    bool no_detach;
    bool container_mode;
    const char *http_port;
    const char *pipe_name;
    const char *unix_path;
    const char *pid_file;
} DaemonStartOptions;

typedef struct {
    bool wait;
    bool force_kill;
} DaemonStopOptions;

/* -------------------------------------------------------------------------- */
/* Config                                                                     */
/* -------------------------------------------------------------------------- */

bool daemon_config_load(DaemonConfig *out);

void daemon_config_apply_defaults(DaemonConfig *cfg);

void daemon_config_apply_start_options(DaemonConfig *cfg, const DaemonStartOptions *opts);

/* -------------------------------------------------------------------------- */
/* Server lifecycle                                                           */
/* -------------------------------------------------------------------------- */

int daemon_start(const DaemonConfig *cfg);

int daemon_spawn_detached(const DaemonConfig *cfg);

int daemon_stop(const DaemonStopOptions *opts);

int daemon_restart(const DaemonConfig *cfg);

int daemon_reload_config(DaemonConfig *cfg);

bool daemon_cleanup_stale_pid_file(const char *path);

bool daemon_is_running(int *pid_out);

int daemon_write_pid_file(const char *path);

int daemon_remove_pid_file(const char *path);

/* -------------------------------------------------------------------------- */
/* Install (platform service registration)                                    */
/* -------------------------------------------------------------------------- */

int daemon_install_systemd(bool user_unit, char *unit_path_out, size_t unit_path_size);

int daemon_uninstall_systemd(bool user_unit);

int daemon_install_launchd(bool user_agent, char *plist_path_out, size_t plist_path_size);

int daemon_uninstall_launchd(bool user_agent);

int daemon_install_windows_service(char *script_path_out, size_t script_path_size);

int daemon_uninstall_windows_service(void);

/* Legacy entry points; prefer daemon_start/stop/restart. */
int start_daemon(void);
int stop_daemon(void);
int restart_daemon(void);

#endif
