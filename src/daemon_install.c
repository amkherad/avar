#include <daemon.h>
#include <file-system.h>
#include <logger.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif

static int write_text_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        LOG_ERROR("Failed to write '%s'", path);
        return -1;
    }
    fputs(content, file);
    fclose(file);
    return 0;
}

int daemon_install_systemd(bool user_unit, char *unit_path_out, size_t unit_path_size) {
#if defined(_WIN32) || defined(__APPLE__)
    (void)user_unit;
    (void)unit_path_out;
    (void)unit_path_size;
    LOG_ERROR("systemd install is only supported on Linux");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char unit_dir[AVAR_CONFIG_PATH_MAX];
    if (user_unit) {
        snprintf(unit_dir, sizeof unit_dir, "%s/.config/systemd/user", home);
    } else {
        snprintf(unit_dir, sizeof unit_dir, "/etc/systemd/system");
    }

    if (make_dirs_in_path(unit_dir) != 0) {
        LOG_ERROR("Failed to create systemd unit directory '%s'", unit_dir);
        return -1;
    }

    char unit_path[AVAR_CONFIG_PATH_MAX];
    snprintf(unit_path, sizeof unit_path, "%s/%s", unit_dir, AVAR_DAEMON_SYSTEMD_UNIT);

    char exec_path[AVAR_CONFIG_PATH_MAX] = "avar";
    const ssize_t len = readlink("/proc/self/exe", exec_path, sizeof exec_path - 1);
    if (len > 0) {
        exec_path[len] = '\0';
    }

    const char *run_user = getenv("USER");
    if (run_user == NULL) {
        run_user = "avar";
    }

    char unit_body[4096];
    snprintf(unit_body, sizeof unit_body,
             "[Unit]\n"
             "Description=Avar Download Manager Daemon\n"
             "After=network-online.target\n"
             "Wants=network-online.target\n"
             "\n"
             "[Service]\n"
             "Type=simple\n"
             "User=%s\n"
             "ExecStart=%s daemon start\n"
             "ExecStop=%s daemon stop --wait\n"
             "ExecReload=%s daemon reload\n"
             "Restart=on-failure\n"
             "NoNewPrivileges=true\n"
             "PrivateTmp=true\n"
             "ProtectSystem=strict\n"
             "ProtectHome=read-only\n"
             "ReadWritePaths=%%h/.config/avar %%h/.local/share/avar /tmp\n"
             "\n"
             "[Install]\n"
             "WantedBy=%s\n",
             run_user, exec_path, exec_path, exec_path,
             user_unit ? "default.target" : "multi-user.target");

    if (write_text_file(unit_path, unit_body) != 0) {
        return -1;
    }

    if (unit_path_out != NULL && unit_path_size > 0) {
        snprintf(unit_path_out, unit_path_size, "%s", unit_path);
    }

    LOG_INFO("Wrote systemd unit to %s", unit_path);
    if (user_unit) {
        LOG_INFO("Enable with: systemctl --user enable --now %s", AVAR_DAEMON_SYSTEMD_UNIT);
    } else {
        LOG_INFO("Enable with: sudo systemctl enable --now %s", AVAR_DAEMON_SYSTEMD_UNIT);
    }
    return 0;
#endif
}

int daemon_uninstall_systemd(bool user_unit) {
#if defined(_WIN32) || defined(__APPLE__)
    (void)user_unit;
    LOG_ERROR("systemd uninstall is only supported on Linux");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char unit_path[AVAR_CONFIG_PATH_MAX];
    if (user_unit) {
        snprintf(unit_path, sizeof unit_path, "%s/.config/systemd/user/%s", home,
                 AVAR_DAEMON_SYSTEMD_UNIT);
    } else {
        snprintf(unit_path, sizeof unit_path, "/etc/systemd/system/%s",
                 AVAR_DAEMON_SYSTEMD_UNIT);
    }

    if (remove(unit_path) != 0) {
        LOG_ERROR("Failed to remove systemd unit at '%s'", unit_path);
        return -1;
    }

    LOG_INFO("Removed systemd unit at %s", unit_path);
    return 0;
#endif
}

int daemon_install_launchd(bool user_agent, char *plist_path_out, size_t plist_path_size) {
#if !defined(__APPLE__)
    (void)user_agent;
    (void)plist_path_out;
    (void)plist_path_size;
    LOG_ERROR("launchd install is only supported on macOS");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char plist_dir[AVAR_CONFIG_PATH_MAX];
    if (user_agent) {
        snprintf(plist_dir, sizeof plist_dir, "%s/Library/LaunchAgents", home);
    } else {
        snprintf(plist_dir, sizeof plist_dir, "/Library/LaunchDaemons");
    }

    if (make_dirs_in_path(plist_dir) != 0) {
        LOG_ERROR("Failed to create launchd directory '%s'", plist_dir);
        return -1;
    }

    char plist_path[AVAR_CONFIG_PATH_MAX];
    snprintf(plist_path, sizeof plist_path, "%s/%s.plist", plist_dir, AVAR_DAEMON_LAUNCHD_LABEL);

    char exec_path[AVAR_CONFIG_PATH_MAX] = "avar";
    uint32_t size = (uint32_t)sizeof exec_path;
    if (_NSGetExecutablePath(exec_path, &size) != 0) {
        exec_path[0] = '\0';
    }

    char plist_body[4096];
    snprintf(plist_body, sizeof plist_body,
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n"
             "<dict>\n"
             "  <key>Label</key><string>%s</string>\n"
             "  <key>ProgramArguments</key>\n"
             "  <array><string>%s</string><string>daemon</string><string>start</string></array>\n"
             "  <key>RunAtLoad</key><true/>\n"
             "  <key>KeepAlive</key><true/>\n"
             "</dict>\n"
             "</plist>\n",
             AVAR_DAEMON_LAUNCHD_LABEL, exec_path[0] != '\0' ? exec_path : "avar");

    if (write_text_file(plist_path, plist_body) != 0) {
        return -1;
    }

    if (plist_path_out != NULL && plist_path_size > 0) {
        snprintf(plist_path_out, plist_path_size, "%s", plist_path);
    }

    LOG_INFO("Wrote launchd plist to %s", plist_path);
    LOG_INFO("Load with: launchctl load %s", plist_path);
    return 0;
#endif
}

int daemon_uninstall_launchd(bool user_agent) {
#if !defined(__APPLE__)
    (void)user_agent;
    LOG_ERROR("launchd uninstall is only supported on macOS");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char plist_path[AVAR_CONFIG_PATH_MAX];
    if (user_agent) {
        snprintf(plist_path, sizeof plist_path, "%s/Library/LaunchAgents/%s.plist", home,
                 AVAR_DAEMON_LAUNCHD_LABEL);
    } else {
        snprintf(plist_path, sizeof plist_path, "/Library/LaunchDaemons/%s.plist",
                 AVAR_DAEMON_LAUNCHD_LABEL);
    }

    if (remove(plist_path) != 0) {
        LOG_ERROR("Failed to remove launchd plist at '%s'", plist_path);
        return -1;
    }

    LOG_INFO("Removed launchd plist at %s", plist_path);
    return 0;
#endif
}

int daemon_install_windows_service(char *script_path_out, size_t script_path_size) {
#if !defined(_WIN32)
    (void)script_path_out;
    (void)script_path_size;
    LOG_ERROR("Windows service install is only supported on Windows");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char script_dir[AVAR_CONFIG_PATH_MAX];
    snprintf(script_dir, sizeof script_dir, "%s\\avar", home);
    if (make_dirs_in_path(script_dir) != 0) {
        return -1;
    }

    char script_path[AVAR_CONFIG_PATH_MAX];
    snprintf(script_path, sizeof script_path, "%s\\install-avar-daemon-service.ps1", script_dir);

    char exec_path[AVAR_CONFIG_PATH_MAX] = {0};
    GetModuleFileNameA(NULL, exec_path, (DWORD)sizeof exec_path);

    char script_body[4096];
    snprintf(script_body, sizeof script_body,
             "$exe = '%s'\n"
             "New-Service -Name 'AvarDaemon' -BinaryPathName \"$exe daemon start --attached\" "
             "-DisplayName 'Avar Download Manager Daemon' -StartupType Automatic\n"
             "Write-Host 'Installed Windows service AvarDaemon'\n",
             exec_path[0] != '\0' ? exec_path : "avar.exe");

    if (write_text_file(script_path, script_body) != 0) {
        return -1;
    }

    if (script_path_out != NULL && script_path_size > 0) {
        snprintf(script_path_out, script_path_size, "%s", script_path);
    }

    LOG_INFO("Wrote Windows service installer script to %s", script_path);
    LOG_INFO("Run in elevated PowerShell: %s", script_path);
    return 0;
#endif
}

int daemon_uninstall_windows_service(void) {
#if !defined(_WIN32)
    LOG_ERROR("Windows service uninstall is only supported on Windows");
    return -1;
#else
    const char *home = get_user_home();
    if (home == NULL) {
        return -1;
    }

    char script_path[AVAR_CONFIG_PATH_MAX];
    snprintf(script_path, sizeof script_path, "%s\\avar\\install-avar-daemon-service.ps1", home);
    if (remove(script_path) != 0) {
        LOG_WARNING("Service script not found at '%s'", script_path);
    }

    LOG_INFO("Remove service with elevated PowerShell: Remove-Service -Name AvarDaemon");
    return 0;
#endif
}
