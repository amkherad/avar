#!/usr/bin/env python
# -*- coding:utf-8 -*-

# Copyright (c) 2012 Vladimir Keleshev, <vladimir@keleshev.com>
# (see LICENSE-MIT file for copying)


"""Usage: docopt_c.py [options] [<docopt>]

Processes a docopt formatted string, from either stdin or a file, and
outputs the equivalent C code to parse a CLI, to either the stdout or a file.

Options:
  -o, --output-name=<outname>
                Filename used to write the produced C file.
                If not present, the produced code is printed to stdout.
  -t, --template=<template>
                Filename used to read a C template.
  -p, --template-header=<template-header>
                Filename used to read a C template header (prototypes, structs).
  -n, --name=<handler-name>
                The cli handler's name.
  -h,--help     Show this help message and exit.

Arguments:
  <docopt>      Input file describing your CLI in docopt language.

"""

__author__ = "Vladimir Keleshev"
__version__ = "2.0rc2"
__description__ = "C generator for language for description of command-line interfaces"

import numbers
import os.path
import re
import textwrap
from string import Template

import sys

import docopt

template_h = """
#ifndef DOCOPT_$header_no_ext_H
#define DOCOPT_$header_no_ext_H

#include <stddef.h>

#if defined(__STDC__) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

#include <stdbool.h>

#elif !defined(_STDBOOL_H)
#define _STDBOOL_H

#include <stdlib.h>

#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif
#ifdef bool
#undef bool
#endif

#define true 1
#define false (!true)
typedef size_t bool;

#endif

#if defined(_AIX)

#include <sys/limits.h>

#elif defined(__FreeBSD__) || defined(__NetBSD__)
|| defined(__OpenBSD__) || defined(__bsdi__)
|| defined(__DragonFly__) || defined(macintosh)
|| defined(__APPLE__) || defined(__APPLE_CC__)

#include <sys/syslimits.h>

#elif defined(__HAIKU__)

#include <system/user_runtime.h>

#elif defined(__linux__) || defined(linux) || defined(__linux)

#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22)

#include <linux/limits.h>

#else

#define ARG_MAX       131072    /* # bytes of args + environ for exec() */
/* it's no longer defined, see this example and more at https://unix.stackexchange.com/q/120642 */

#endif

#elif (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) \
 || defined(__DragonFly__) || defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__))

#include <sys/param.h>

#if defined(__APPLE__) || defined(__APPLE_CC__)
/* ARG_MAX gives a segfault on macOS when used for array size below */
#undef ARG_MAX
#undef NCARGS
#endif

#else

#include <limits.h>

#endif

#ifndef ARG_MAX
#ifdef NCARGS
#define ARG_MAX NCARGS
#else
#define ARG_MAX 131072
#endif
#endif

struct ${DocoptArgs} {
    $commands$arguments$flags$options
    /* special */
    const char *usage_pattern;
    const int help_message_n;
    const char *help_message[$help_message_n];
};

struct ${DocoptArgs} ${docopt_func}(int, char *[], bool, const char *);

#endif
"""

template_c = """
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cli.h>
#include "$header_name"

const char $usage_pattern_var_name[] =
        $usage_pattern;

int ${elems_to_args_name}(struct Elements *elements, struct ${DocoptArgs} *args,
                     const bool help, const char *version) {
    struct Command *command;
    struct Argument *argument;
    struct Option *option;
    int i, j;

    /* fix gcc-related compiler warnings (unused) */
    (void) command;
    (void) argument;

    /* options */
    for (i = 0; i < elements->n_options; i++) {
        option = &elements->options[i];
        if (help && option->value && strcmp(option->olong, "--help") == 0) {
            for (j = 0; j < $help_message_n; j++)
                puts(args->help_message[j]);
            return EXIT_FAILURE;
        } else if (version && option->value &&
                   strcmp(option->olong, "--version") == 0) {
            puts(version);
            return EXIT_FAILURE;
        }$if_flag$if_option
    }
    /* commands */
    for (i = 0; i < elements->n_commands; i++) {
        command = &elements->commands[i];
        $if_command
    }
    /* arguments */
    for (i = 0; i < elements->n_arguments; i++) {
        argument = &elements->arguments[i];
        $if_argument
    }
    return EXIT_SUCCESS;
}

/*
 * Main docopt function
 */

struct ${DocoptArgs} ${docopt_func}(int argc, char *argv[], const bool help, const char *version) {
    struct ${DocoptArgs} args = {$defaults
            ${usage_pattern_var_name},
            $help_message_n,
            $help_message
    };
    struct Command commands[] = {$elems_cmds
    };
    struct Argument arguments[] = {$elems_args
    };
    struct Option options[] = {$elems_opts
    };
    struct Elements elements;
    int return_code = EXIT_SUCCESS;

    elements.n_commands = $t_elems_n_commands;
    elements.n_arguments = $t_elems_n_arguments;
    elements.n_options = $t_elems_n_options;
    elements.commands = commands;
    elements.arguments = arguments;
    elements.options = options;

    if (argc == 1) {
        argv[argc++] = "--help";
        argv[argc++] = NULL;
        return_code = EXIT_FAILURE;
    }

    {
        struct Tokens ts = tokens_new(argc, argv);
        if (parse_args(&ts, &elements))
            exit(EXIT_FAILURE);
    }
    if (${elems_to_args_name}(&elements, &args, help, version))
        exit(return_code);
    return args;
}
"""

def to_initializer(val):
    if isinstance(val, (str, type(None), bool, numbers.Number)):
        return to_c(val)

    return '{{{}}}'.format(',\n'.join('{}"{}"'.format(' ' * 10, k) for k in val)[9:])


def to_c(val):
    if type(val) is str:
        return '"{}"'.format(val
                             .replace('\\', r'\\')
                             .replace('"', r'\"')
                             .replace('\n', '\\n"\n"')
                             )
    elif val is True:
        return '1'
    elif val is False:
        return '0'
    elif isinstance(val, numbers.Number):
        return str(val)
    elif val is None:
        return 'NULL'
    raise ValueError("can't convert to c type: {!r}".format(val))


def c_command(obj):
    return '{{{!s}}}'.format(', '.join(to_c(v)
                                       for v in (obj.name, obj.value)))


def c_argument(obj):
    return '{{{!s}}}'.format(', '.join(to_c(v)
                                       for v in (obj.name, obj.value, None)))


def c_option(obj):
    return '{{{!s}}}'.format(', '.join(to_c(v)
                                       for v in (obj.short, obj.long, obj.argcount,
                                                 False, None)))


def c_name(s):
    return ''.join(c if c.isalnum() else '_' for c in s).strip('_')


def c_if_command(cmd):
    return 'if (strcmp(command->name, {val!s}) == 0) {{\n' \
           '    args->{prop!s} = command->value;\n' \
           '}}\n'.format(val=to_c(cmd.name),
                         prop=c_name(cmd.name))


def c_if_argument(arg):
    return 'if (strcmp(argument->name, {val!s}) == 0) {{\n' \
           '    args->{prop!s} = (char *) argument->value;\n' \
           '}}\n'.format(val=to_c(arg.name),
                         prop=c_name(arg.name))


def c_if_flag(obj):
    return ' else if (strcmp(option->o{typ!s}, {val!s}) == 0) {{\n' \
           '    args->{prop!s} = option->value;\n' \
           '}}\n'.format(typ=('long' if obj.long else 'short'),
                         val=to_c(obj.long or obj.short),
                         prop=c_name(obj.long or obj.short))


def c_if_option(obj):
    return ' else if (strcmp(option->o{typ!s}, {val!s}) == 0) {{\n' \
           '    if (option->argument) {{\n' \
           '        args->{prop!s} = (char *) option->argument;\n' \
           '    }}\n}}\n'.format(typ=('long' if obj.long else 'short'),
                                 val=to_c(obj.long or obj.short),
                                 prop=c_name(obj.long or obj.short))


def parse_leafs(pattern, all_options):
    options_shortcut = False
    leaves = []
    queue = [(0, pattern)]
    while queue:
        level, node = queue.pop(-1)  # depth-first search
        if not options_shortcut and type(node) == docopt.OptionsShortcut:
            options_shortcut = True
        elif hasattr(node, 'children'):
            children = [(level + 1, child) for child in node.children]
            children.reverse()
            queue.extend(children)
        else:
            if node not in leaves:
                leaves.append(node)
    sort_by_name = lambda e: e.name
    leaves.sort(key=sort_by_name)
    commands = [leaf for leaf in leaves if type(leaf) == docopt.Command]
    arguments = [leaf for leaf in leaves if type(leaf) == docopt.Argument]
    if options_shortcut:
        option_leafs = all_options
        option_leafs.sort(key=sort_by_name)
    else:
        option_leafs = [leaf for leaf in leaves if type(leaf) == docopt.Option]
    flags = [leaf for leaf in option_leafs if leaf.argcount == 0]
    options = [leaf for leaf in option_leafs if leaf.argcount > 0]
    leaves = [i for sl in (commands, arguments, flags, options) for i in sl]
    return leaves, commands, arguments, flags, options


def null_if_zero(s):
    return 'NULL' if s is None or len(s) == 0 else s


def main():
    assert __doc__ is not None
    args = docopt.docopt(__doc__)

    try:
        if args['<docopt>'] is not None:
            with open(args['<docopt>'], 'r') as f:
                args['<docopt>'] = f.read()
        elif args['<docopt>'] is None and sys.stdin.isatty():
            print(__doc__.strip("\n"))
            sys.exit("")
        else:
            args['<docopt>'] = sys.stdin.read()
        if args['--template'] is None:
            args['--template'] = template_c
        else:
            with open(args['--template'], 'rt') as f:
                args['--template'] = f.read()
        if args['--template-header'] is None:
            args['--template-header'] = template_h
        else:
            with open(args['--template-header'], 'rt') as f:
                args['--template-header'] = f.read()
    except IOError as e:
        sys.exit(e)

    doc = args['<docopt>']
    usage = docopt.parse_section('usage:', doc)
    error_str_l = 'More than one ', '"usage:" (case-insensitive)', ' not found.'
    usage = {0: error_str_l[1:], 1: usage[0] if usage else None}.get(len(usage), error_str_l[:2])
    if isinstance(usage, list):
        raise docopt.DocoptLanguageError(''.join(usage))

    all_options = docopt.parse_defaults(doc)
    pattern = docopt.parse_pattern(docopt.formal_usage(usage), all_options)
    leafs, commands, arguments, flags, options = parse_leafs(pattern, all_options)

    _indent = ' ' * 4

    t_commands = ';\n{indent}'.format(indent=_indent).join('size_t {!s}'.format(c_name(cmd.name))
                                                           for cmd in commands)
    t_commands = '\n{indent}/* commands */\n{indent}{t_commands};'.format(indent=_indent, t_commands=t_commands) \
        if t_commands != '' else ''
    t_arguments = ';\n{indent}'.format(indent=_indent).join('char *{!s}'.format(c_name(arg.name))
                                     for arg in arguments)
    t_arguments = '\n{indent}/* arguments */\n{indent}{t_arguments};'.format(indent=_indent, t_arguments=t_arguments) \
        if t_arguments != '' else ''
    t_flags = ';\n{indent}'.format(indent=_indent).join('size_t {!s}'.format(c_name(flag.long or flag.short))
                                                        for flag in flags)
    t_flags = '\n{indent}/* options without arguments */\n{indent}{t_flags};'.format(indent=_indent, t_flags=t_flags) \
        if t_flags != '' else ''
    t_options = ';\n{indent}'.format(indent=_indent).join('char *{!s}'.format(c_name(opt.long or opt.short))
                                                          for opt in options)
    t_options = '\n{indent}/* options with arguments */\n{indent}{t_options};'.format(indent=_indent,
                                                                                      t_options=t_options) \
        if t_options != '' else ''
    t_defaults = ', '.join(to_c(leaf.value) for leaf in leafs)
    t_defaults = re.sub('"(.*?)"', '(char *) "\1"', t_defaults)
    t_defaults = '\n{indent}'.format(indent=_indent * 2).join(textwrap.wrap(t_defaults, 72))
    t_defaults = '\n{indent}{t_defaults},'.format(indent=_indent * 2, t_defaults=t_defaults) if t_defaults != '' else ''
    t_elems_cmds = ',\n{indent}'.format(indent=_indent * 2).join(c_command(cmd) for cmd in commands)
    t_elems_cmds = '\n{indent}{t_elems_cmds}'.format(indent=_indent * 2,
                                                     t_elems_cmds=t_elems_cmds) if t_elems_cmds != '' else ''
    t_elems_args = ',\n{indent}'.format(indent=_indent * 2).join(c_argument(arg) for arg in arguments)
    t_elems_args = '\n{indent}{t_elems_args}'.format(indent=_indent * 2,
                                                     t_elems_args=t_elems_args) if t_elems_args != '' else ''
    t_elems_opts = ',\n{indent}'.format(indent=_indent * 2).join(c_option(o) for o in (flags + options))
    t_elems_opts = '\n{indent}{t_elems_opts}'.format(indent=_indent * 2,
                                                     t_elems_opts=t_elems_opts) if t_elems_opts != '' else ''

    '''
    t_elems_n_commands = str(len(commands))
    t_elems_n_arguments = str(len(arguments))
    t_elems_n_options = str(len(flags + options))
    t_elems_n_cmds = str(len(commands))
    t_elems_n = ', '.join(str(len(l)) for l in (commands, arguments, (flags + options)))
    print(
        't_elems_n_commands:', t_elems_n_commands, ';\n',
        't_elems_n_arguments:', t_elems_n_arguments, ';\n',
        't_elems_n_options:', t_elems_n_options, ';\n',
        't_elems_n_cmds:', t_elems_n_cmds, ';\n',
        't_elems_n:', t_elems_n, ';'
    )
    '''

    t_if_command = ' else '.join(c_if_command(command) for command in commands)
    t_if_command = '\n{indent}{t_if_command}'.format(indent=_indent * 2,
                                                     t_if_command=t_if_command) if t_if_command != '' else ''
    t_if_argument = ' else '.join(c_if_argument(arg) for arg in arguments)
    t_if_argument = '\n{indent}{t_if_argument}'.format(
        indent=_indent * 2,
        t_if_argument='\n{indent}'.format(indent=_indent * 2).join(t_if_argument.splitlines())
    ) if t_if_argument != '' else ''
    t_if_flag = ''.join('\n{indent}'.format(indent=_indent * 2).join(c_if_flag(flag).splitlines())
                        for flag in flags)
    t_if_option = ''.join(
        '\n{indent}'.format(indent=_indent * 2).join(c_if_option(opt).splitlines())
        for opt in options
    )

    if not args['--output-name']:
        header_output_name = '<stdout>'
    else:
        base, ext = os.path.splitext(args['--output-name'])
        if ext not in frozenset(('.h', '.c')):
            base = args['--output-name']

        args['--output-name'] = "{base}.c".format(base=base)
        header_output_name = "{base}.h".format(base=base)

    header_name = os.path.basename(header_output_name)

    if args['--name'] is None:
        command_name = ''
    else:
        command_name = args['--name']

    doc = doc.splitlines()
    doc_n = len(doc)

    template_out = Template(args['--template']).safe_substitute(
        help_message='\n{indent}'.format(indent=_indent).join(to_initializer(doc).splitlines()),
        help_message_n=doc_n,
        usage_pattern='\n{indent}'.format(indent=_indent * 2).join(to_c(usage).splitlines()),
        if_flag=t_if_flag,
        if_option=t_if_option,
        if_command=t_if_command,
        if_argument=t_if_argument,
        defaults=t_defaults,
        elems_cmds=null_if_zero(t_elems_cmds),
        elems_args=null_if_zero(t_elems_args),
        elems_opts=null_if_zero(t_elems_opts),
        t_elems_n_commands=str(len(commands)),
        t_elems_n_arguments=str(len(arguments)),
        t_elems_n_options=str(len(flags + options)),
        header_name=header_name,
        DocoptArgs='{command_name}_DocoptArgs'.format(command_name=command_name),
        docopt_func='parse_{command_name}_docopt'.format(command_name=command_name),
        usage_pattern_var_name='{command_name}_usage_pattern'.format(command_name=command_name),
        elems_to_args_name='{command_name}_elems_to_args'.format(command_name=command_name)
    )

    template_header_out = Template(args['--template-header']).safe_substitute(
        commands=t_commands,
        arguments=t_arguments,
        flags=t_flags,
        options=t_options,
        help_message_n=doc_n,
        DocoptArgs='{command_name}_DocoptArgs'.format(command_name=command_name),
        docopt_func='parse_{command_name}_docopt'.format(command_name=command_name),
        # nargs=t_nargs
    ).replace('$header_no_ext', os.path.splitext(header_name)[0].upper())

    if args['--output-name'] is None:
        print(template_out.strip(), '\n')
    else:
        try:
            with open(sys.stdout if args['--output-name'] in (None, "<stdout>") else args['--output-name'], 'w') as f:
                f.write(template_out.strip() + '\n')

            with open(sys.stdout if header_output_name == "<stdout>" else header_output_name, 'w') as f:
                f.write(template_header_out.strip() + '\n')

        except IOError as e:
            sys.exit(str(e))


if __name__ == '__main__':
    main()
