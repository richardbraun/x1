/*
 * Copyright (c) 2018 Richard Braun.
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
 */

#ifndef SHELL_I_H
#define SHELL_I_H

#include <stddef.h>

#include <lib/macros.h>

#include <src/mutex.h>

struct shell_cmd {
    struct shell_cmd *ht_next;
    struct shell_cmd *ls_next;
    const char *name;
    shell_fn_t fn;
    const char *usage;
    const char *short_desc;
    const char *long_desc;
};

struct shell_bucket {
    struct shell_cmd *cmd;
};

/*
 * Binary exponent and size of the hash table used to store commands.
 */
#define SHELL_HTABLE_BITS   6
#define SHELL_HTABLE_SIZE   (1 << SHELL_HTABLE_BITS)

/*
 * The command list is sorted.
 */
struct shell_cmd_set {
    struct mutex lock;
    struct shell_bucket htable[SHELL_HTABLE_SIZE];
    struct shell_cmd *cmd_list;
};

#define SHELL_LINE_MAX_SIZE 64

/*
 * Line containing a shell entry.
 *
 * The string must be nul-terminated. The size doesn't include this
 * additional nul character, the same way strlen() doesn't account for it.
 */
struct shell_line {
    char str[SHELL_LINE_MAX_SIZE];
    size_t size;
};

/*
 * Number of entries in the history.
 *
 * One of these entryes is used as the current line.
 */
#define SHELL_HISTORY_SIZE 21

#if SHELL_HISTORY_SIZE == 0
#error "shell history size must be non-zero"
#endif /* SHELL_HISTORY_SIZE == 0 */

/*
 * Shell history.
 *
 * The history is never empty. There is always at least one entry, the
 * current line, referenced by the newest (most recent) index. The array
 * is used like a circular buffer, i.e. old entries are implicitely
 * erased by new ones. The index references the entry used as a template
 * for the current line.
 */
struct shell_history {
    struct shell_line lines[SHELL_HISTORY_SIZE];
    size_t newest;
    size_t oldest;
    size_t index;
};

/*
 * This value changes depending on the standard used and was chosen arbitrarily.
 */
#define SHELL_ESC_SEQ_MAX_SIZE 8

#define SHELL_MAX_ARGS 16

/*
 * Shell structure.
 *
 * A shell instance can include temporary variables to minimize stack usage.
 */
struct shell {
    struct shell_cmd_set *cmd_set;

    shell_getc_fn_t getc_fn;
    shell_vfprintf_fn_t vfprintf_fn;
    void *io_object;

    struct shell_history history;

    /* Cursor within the current line */
    size_t cursor;

    /* Members used for escape sequence parsing */
    char esc_seq[SHELL_ESC_SEQ_MAX_SIZE];
    size_t esc_seq_index;

    /*
     * Buffer used to store the current line during argument processing.
     *
     * The pointers in the argv array point inside this buffer. The
     * separators immediately following the arguments are replaced with
     * null characters.
     */
    char tmp_line[SHELL_LINE_MAX_SIZE];

    int argc;
    char *argv[SHELL_MAX_ARGS];
};

#endif /* SHELL_I_H */
