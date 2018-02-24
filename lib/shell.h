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

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <lib/macros.h>

#include <src/panic.h>

#define SHELL_REGISTER_CMDS(cmds)                           \
MACRO_BEGIN                                                 \
    size_t i___;                                            \
    int error___;                                           \
                                                            \
    for (i___ = 0; i___ < ARRAY_SIZE(cmds); i___++) {       \
        error___ = shell_cmd_register(&(cmds)[i___]);       \
                                                            \
        if (error___) {                                     \
            panic("%s: %s", __func__, strerror(error___));  \
        }                                                   \
    }                                                       \
MACRO_END

typedef void (*shell_fn_t)(int argc, char *argv[]);

struct shell_cmd {
    struct shell_cmd *ht_next;
    struct shell_cmd *ls_next;
    const char *name;
    shell_fn_t fn;
    const char *usage;
    const char *short_desc;
    const char *long_desc;
};

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
 * Initialize the shell module.
 *
 * On return, shell commands can be registered.
 */
void shell_setup(void);

/*
 * Register a shell command.
 *
 * The command name must be unique. It must not include characters outside
 * the [a-zA-Z0-9-_] class.
 *
 * The structure passed when calling this function is directly reused by
 * the shell module and must persist in memory.
 */
int shell_cmd_register(struct shell_cmd *cmd);

#endif /* SHELL_H */
