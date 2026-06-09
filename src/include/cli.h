#ifndef AVAR_CLI_H
#define AVAR_CLI_H

#include "utils.h"

#include <argtable3.h>
#include <stdbool.h>

#define EXIT_UNKNOWN_COMMAND 2
#define EXIT_FATAL_ERROR 3

typedef struct {
    stringa name;
    int (*handler)(int argc, char *argv[]);
} Command;

typedef struct {
    bool help;
    bool version;
    const char *url;
    const char *queue;
    const char *name;
    bool detached;
    bool attached;
} AvarArgs;

int cli_run(int argc, char *argv[]);

int cli_run_argtable(const char *progname, void **argtable, arg_end_t *end, int argc, char **argv, bool *help_out);

int cli_make_subargv(int argc, char **argv, int skip, const char *progname, int *sub_argc, char ***sub_argv);

void cli_free_subargv(char **sub_argv);

void cli_print_avar_help(void);

bool cli_is_help_flag(const char *arg);

void cli_print_argtable_help(const char *progname, void **argtable);

bool cli_try_command_help(int argc, char **argv, int help_index, void (*print_help)(void));

int cli_parse_avar(int argc, char **argv, AvarArgs *out);

int handle_download(int argc, char *argv[]);

int handle_queue(int argc, char *argv[]);

int handle_config(int argc, char *argv[]);

int handle_profile(int argc, char *argv[]);

int handle_scheduler(int argc, char *argv[]);

int handle_daemon(int argc, char *argv[]);

#endif
