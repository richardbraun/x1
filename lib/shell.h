/*
 * Copyright (c) 2015-2018 Richard Braun.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Upstream site with license notes :
 * http://git.sceen.net/rbraun/librbraun.git/
 *
 *
 * Minimalist shell for embedded systems.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/macros.h>

/*
 * Types for I/O functions.
 */
typedef int (*shell_getc_fn_t)(void *io_object);
typedef void (*shell_vfprintf_fn_t)(void *io_object,
                                    const char *format, va_list ap);

/*
 * Shell structure, statically allocatable.
 */
struct shell;

/*
 * Shell command structure.
 */
struct shell_cmd;

/*
 * Command container, shareable across multiple shell instances.
 */
struct shell_cmd_set;

/*
 * Type for command implementation callbacks.
 */
typedef void (*shell_fn_t)(struct shell *shell, int argc, char **argv);

#include "shell_i.h"

#define SHELL_REGISTER_CMDS(cmds, cmd_set)                              \
MACRO_BEGIN                                                             \
    size_t i_;                                                          \
    int error_;                                                         \
                                                                        \
    for (i_ = 0; i_ < ARRAY_SIZE(cmds); i_++) {                         \
        error_ = shell_cmd_set_register(cmd_set, &(cmds)[i_]);          \
                                                                        \
        if (error_) {                                                   \
            panic("%s: %s\n", __func__, strerror(error_));              \
        }                                                               \
    }                                                                   \
MACRO_END

/*
 * Static shell command initializers.
 */
#define SHELL_CMD_INITIALIZER(name, fn, usage, short_desc) \
    { NULL, NULL, name, fn, usage, short_desc, NULL }
#define SHELL_CMD_INITIALIZER2(name, fn, usage, short_desc, long_desc) \
    { NULL, NULL, name, fn, usage, short_desc, long_desc }

/*
 * Initialize a shell command structure.
 */
void shell_cmd_init(struct shell_cmd *cmd, const char *name,
                    shell_fn_t fn, const char *usage,
                    const char *short_desc, const char *long_desc);

/*
 * Initialize a command set.
 */
void shell_cmd_set_init(struct shell_cmd_set *cmd_set);

/*
 * Register a shell command.
 *
 * The command name must be unique. It must not include characters outside
 * the [a-zA-Z0-9-_] class.
 *
 * Commands may safely be registered while the command set is used.
 *
 * The command structure must persist in memory as long as the command set
 * is used.
 */
int shell_cmd_set_register(struct shell_cmd_set *cmd_set,
                           struct shell_cmd *cmd);

/*
 * Initialize a shell instance.
 *
 * On return, shell commands can be registered.
 */
void shell_init(struct shell *shell, struct shell_cmd_set *cmd_set,
                shell_getc_fn_t getc_fn, shell_vfprintf_fn_t vfprintf_fn,
                void *io_object);

/*
 * Run the shell.
 *
 * This function doesn't return.
 */
void shell_run(struct shell *shell);

/*
 * Obtain the command set associated with a shell.
 */
struct shell_cmd_set * shell_get_cmd_set(struct shell *shell);

/*
 * Printf-like functions specific to the given shell instance.
 */
void shell_printf(struct shell *shell, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void shell_vprintf(struct shell *shell, const char *format, va_list ap)
    __attribute__((format(printf, 2, 0)));

#endif /* SHELL_H */
