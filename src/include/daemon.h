#ifndef AVAR_DAEMON_H
#define AVAR_DAEMON_H

int handle_daemon(int argc, char *argv[]);

int start_daemon();
int stop_daemon();
int restart_daemon();

#endif
