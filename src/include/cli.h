#ifndef AVAR_CLI_H
#define AVAR_CLI_H

#include "utils.h"

#include <avar_cli_handler.h>
#include <batch_cli_handler.h>
#include <config_cli_handler.h>
#include <download_cli_handler.h>
#include <profile_cli_handler.h>
#include <queue_cli_handler.h>

#define EXIT_UNKNOWN_COMMAND 2
#define EXIT_FATAL_ERROR 3

struct Command {
    stringa name;
    bool value;
};

struct Argument {
    stringa name;
    stringa value;
    stringa *array;
};

struct Option {
    stringa oshort;
    stringa olong;
    bool argcount;
    bool value;
    stringa argument;
};

struct Elements {
    int n_commands;
    int n_arguments;
    int n_options;
    struct Command *commands;
    struct Argument *arguments;
    struct Option *options;
};


/*
 * Tokens object
 */

struct Tokens {
    int argc;
    char **argv;
    int i;
    char *current;
};

struct Tokens tokens_new(int argc, char **argv);

struct Tokens *tokens_move(struct Tokens *ts);

int parse_doubledash(struct Tokens *ts, struct Elements *elements);

int parse_long(struct Tokens *ts, struct Elements *elements);

int parse_shorts(struct Tokens *ts, struct Elements *elements);

int parse_argcmd(struct Tokens *ts, struct Elements *elements);

int parse_args(struct Tokens *ts, struct Elements *elements);

typedef struct {
    stringa name;

    int (*handler)(int argc, char *argv[]);
} Command;

int handle_scheduler(int argc, char *argv[]);

int handle_batch(int argc, char *argv[]);

// int handle_daemon(int argc, char *argv[]);

#endif
