#include <cli.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct Tokens tokens_new(int argc, char **argv) {
    struct Tokens ts;
    ts.argc = argc;
    ts.argv = argv;
    ts.i = 0;
    ts.current = argv[0];
    return ts;
}

struct Tokens *tokens_move(struct Tokens *ts) {
    if (ts->i < ts->argc) {
        ts->current = ts->argv[++ts->i];
    }
    if (ts->i == ts->argc) {
        ts->current = nullptr;
    }
    return ts;
}


/*
 * ARGV parsing functions
 */

int parse_doubledash(struct Tokens *ts, struct Elements *elements) {
    /*
    int n_commands = elements->n_commands;
    int n_arguments = elements->n_arguments;
    Command *commands = elements->commands;
    Argument *arguments = elements->arguments;

    not implemented yet
    return parsed + [Argument(None, v) for v in tokens]
    */
    return 0;
}

int parse_long(struct Tokens *ts, struct Elements *elements) {
    int i;
    int len_prefix;
    int n_options = elements->n_options;
    char *eq = strchr(ts->current, '=');
    struct Option *option;
    struct Option *options = elements->options;

    len_prefix = (eq - (ts->current)) / sizeof(char);
    for (i = 0; i < n_options; i++) {
        option = &options[i];
        if (!strncmp(ts->current, option->olong, len_prefix))
            break;
    }
    if (i == n_options) {
        /* TODO: %s is not a unique prefix */
        fprintf(stderr, "%s is not recognized\n", ts->current);
        return 1;
    }
    tokens_move(ts);
    if (option->argcount) {
        if (eq == nullptr) {
            if (ts->current == NULL) {
                fprintf(stderr, "%s requires argument\n", option->olong);
                return 1;
            }
            option->argument = ts->current;
            tokens_move(ts);
        } else {
            option->argument = eq + 1;
        }
    } else {
        if (eq != NULL) {
            fprintf(stderr, "%s must not have an argument\n", option->olong);
            return 1;
        }
        option->value = true;
    }
    return 0;
}


int parse_shorts(struct Tokens *ts, struct Elements *elements) {
    char *raw;
    int i;
    int n_options = elements->n_options;
    struct Option *option;
    struct Option *options = elements->options;

    raw = &ts->current[1];
    tokens_move(ts);
    while (raw[0] != '\0') {
        for (i = 0; i < n_options; i++) {
            option = &options[i];
            if (option->oshort != NULL && option->oshort[1] == raw[0])
                break;
        }
        if (i == n_options) {
            /* TODO -%s is specified ambiguously %d times */
            fprintf(stderr, "-%c is not recognized\n", raw[0]);
            return EXIT_FAILURE;
        }
        raw++;
        if (!option->argcount) {
            option->value = true;
        } else {
            if (raw[0] == '\0') {
                if (ts->current == NULL) {
                    fprintf(stderr, "%s requires argument\n", option->oshort);
                    return EXIT_FAILURE;
                }
                raw = ts->current;
                tokens_move(ts);
            }
            option->argument = raw;
            break;
        }
    }
    return EXIT_SUCCESS;
}

int parse_argcmd(struct Tokens *ts, struct Elements *elements) {
    int i;
    int n_commands = elements->n_commands;
    /* int n_arguments = elements->n_arguments; */
    struct Command *command;
    struct Command *commands = elements->commands;
    /* Argument *arguments = elements->arguments; */

    for (i = 0; i < n_commands; i++) {
        command = &commands[i];
        if (strcmp(command->name, ts->current) == 0) {
            command->value = true;
            tokens_move(ts);
            return EXIT_SUCCESS;
        }
    }
    /* not implemented yet, just skip for now
       parsed.append(Argument(None, tokens.move())) */
    /*
    fprintf(stderr, "! argument '%s' has been ignored\n", ts->current);
    fprintf(stderr, "  '");
    for (i=0; i<ts->argc ; i++)
        fprintf(stderr, "%s ", ts->argv[i]);
    fprintf(stderr, "'\n");
    */
    tokens_move(ts);
    return EXIT_SUCCESS;
}

int parse_args(struct Tokens *ts, struct Elements *elements) {
    int ret = EXIT_FAILURE;

    while (ts->current != nullptr) {
        if (strcmp(ts->current, "--") == 0) {
            ret = parse_doubledash(ts, elements);
            if (ret == EXIT_FAILURE) break;
        } else if (ts->current[0] == '-' && ts->current[1] == '-') {
            ret = parse_long(ts, elements);
        } else if (ts->current[0] == '-' && ts->current[1] != '\0') {
            ret = parse_shorts(ts, elements);
        } else
            ret = parse_argcmd(ts, elements);
        if (ret) return ret;
    }
    return ret;
}
