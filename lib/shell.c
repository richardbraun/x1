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
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <lib/macros.h>
#include <lib/hash.h>
#include <lib/shell.h>

#include <src/mutex.h>
#include <src/panic.h>
#include <src/thread.h>
#include <src/uart.h>

#define SHELL_STACK_SIZE    4096

/*
 * Binary exponent and size of the hash table used to store commands.
 */
#define SHELL_HTABLE_BITS   6
#define SHELL_HTABLE_SIZE   (1 << SHELL_HTABLE_BITS)

struct shell_bucket {
    struct shell_cmd *cmd;
};

/*
 * Hash table for quick command lookup.
 */
static struct shell_bucket shell_htable[SHELL_HTABLE_SIZE];

#define SHELL_COMPLETION_MATCH_FMT              "-16s"
#define SHELL_COMPLETION_NR_MATCHES_PER_LINE    4

/*
 * Sorted command list.
 */
static struct shell_cmd *shell_list;

/*
 * Lock protecting access to the hash table and list of commands.
 *
 * Note that this lock only protects access to the commands containers,
 * not the commands themselves. In particular, it is not necessary to
 * hold this lock when a command is used, i.e. when accessing a command
 * name, function pointer, or description.
 */
static struct mutex shell_lock;

/*
 * Escape sequence states.
 *
 * Here is an incomplete description of escape sequences :
 * http://en.wikipedia.org/wiki/ANSI_escape_code
 *
 * These values must be different from 0.
 */
#define SHELL_ESC_STATE_START   1
#define SHELL_ESC_STATE_CSI     2

/*
 * This value changes depending on the standard used and was chosen arbitrarily.
 */
#define SHELL_ESC_SEQ_MAX_SIZE 8

typedef void (*shell_esc_seq_fn)(void);

struct shell_esc_seq {
    const char *str;
    shell_esc_seq_fn fn;
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
    unsigned long size;
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
static struct shell_line shell_history[SHELL_HISTORY_SIZE];
static unsigned long shell_history_newest;
static unsigned long shell_history_oldest;
static unsigned long shell_history_index;

/*
 * Cursor within the current line.
 */
static unsigned long shell_cursor;

#define SHELL_SEPARATOR ' '

/*
 * Commonly used backspace control characters.
 *
 * XXX Adjust for your needs.
 */
#define SHELL_ERASE_BS  '\b'
#define SHELL_ERASE_DEL '\x7f'

/*
 * Buffer used to store the current line during argument processing.
 *
 * The pointers in the argv array point inside this buffer. The
 * separators immediately following the arguments are replaced with
 * nul characters.
 */
static char shell_tmp_line[SHELL_LINE_MAX_SIZE];

#define SHELL_MAX_ARGS 16

static int shell_argc;
static char *shell_argv[SHELL_MAX_ARGS];

static const char *
shell_find_word(const char *str)
{
    for (;;) {
        if ((*str == '\0') || (*str != SHELL_SEPARATOR)) {
            break;
        }

        str++;
    }

    return str;
}

void
shell_cmd_init(struct shell_cmd *cmd, const char *name,
               shell_fn_t fn, const char *usage,
               const char *short_desc, const char *long_desc)
{
    cmd->ht_next = NULL;
    cmd->ls_next = NULL;
    cmd->name = name;
    cmd->fn = fn;
    cmd->usage = usage;
    cmd->short_desc = short_desc;
    cmd->long_desc = long_desc;
}

static const char *
shell_cmd_name(const struct shell_cmd *cmd)
{
    return cmd->name;
}

static inline struct shell_bucket *
shell_bucket_get(const char *name)
{
    return &shell_htable[hash_str(name, SHELL_HTABLE_BITS)];
}

static void
shell_cmd_acquire(void)
{
    mutex_lock(&shell_lock);
}

static void
shell_cmd_release(void)
{
    mutex_unlock(&shell_lock);
}

static const struct shell_cmd *
shell_cmd_lookup(const char *name)
{
    const struct shell_bucket *bucket;
    const struct shell_cmd *cmd;

    shell_cmd_acquire();

    bucket = shell_bucket_get(name);

    for (cmd = bucket->cmd; cmd != NULL; cmd = cmd->ht_next) {
        if (strcmp(cmd->name, name) == 0) {
            break;
        }
    }

    shell_cmd_release();

    return cmd;
}

/*
 * Look up the first command that matches a given string.
 *
 * The input string is defined by the given string pointer and size.
 *
 * The global lock must be acquired before calling this function.
 */
static const struct shell_cmd *
shell_cmd_match(const struct shell_cmd *cmd, const char *str,
                unsigned long size)
{
    while (cmd != NULL) {
        if (strncmp(cmd->name, str, size) == 0) {
            return cmd;
        }

        cmd = cmd->ls_next;
    }

    return NULL;
}

/*
 * Attempt command auto-completion.
 *
 * The given string is the beginning of a command, or the empty string.
 * The sizep parameter initially points to the size of the given string.
 * If the string matches any registered command, the cmdp pointer is
 * updated to point to the first matching command in the sorted list of
 * commands, and sizep is updated to the number of characters in the
 * command name that are common in subsequent commands. The command
 * pointer and the returned size can be used to print a list of commands
 * eligible for completion.
 *
 * If there is a single match for the given string, return 0. If there
 * are more than one match, return EAGAIN. If there is no match,
 * return EINVAL.
 *
 * The global lock must be acquired before calling this function.
 */
static int
shell_cmd_complete(const char *str, unsigned long *sizep,
                   const struct shell_cmd **cmdp)
{
    const struct shell_cmd *cmd, *next;
    unsigned long size;

    size = *sizep;

    /*
     * Start with looking up a command that matches the given argument.
     * If there is no match, return an error.
     */
    cmd = shell_cmd_match(shell_list, str, size);

    if (cmd == NULL) {
        return EINVAL;
    }

    *cmdp = cmd;

    /*
     * If at least one command matches, try to complete it.
     * There can be two cases :
     * 1/ There is one and only one match, which is directly returned.
     * 2/ There are several matches, in which case the common length is
     *    computed.
     */
    next = cmd->ls_next;

    if ((next == NULL)
        || (strncmp(cmd->name, next->name, size) != 0)) {
        *sizep = strlen(cmd->name);
        return 0;
    }

    /*
     * When computing the common length, all the commands that can match
     * must be evaluated. Considering the current command is the first
     * that can match, the only other variable missing is the last
     * command that can match.
     */
    while (next->ls_next != NULL) {
        if (strncmp(cmd->name, next->ls_next->name, size) != 0) {
            break;
        }

        next = next->ls_next;
    }

    if (size == 0) {
        size = 1;
    }

    while ((cmd->name[size - 1] != '\0')
           && (cmd->name[size - 1] == next->name[size - 1])) {
        size++;
    }

    size--;
    *sizep = size;
    return EAGAIN;
}

/*
 * Print a list of commands eligible for completion, starting at the
 * given command. Other eligible commands share the same prefix, as
 * defined by the size argument.
 *
 * The global lock must be acquired before calling this function.
 */
static void
shell_cmd_print_matches(const struct shell_cmd *cmd, unsigned long size)
{
    const struct shell_cmd *tmp;
    unsigned int i;

    printf("\n");

    for (tmp = cmd, i = 1; tmp != NULL; tmp = tmp->ls_next, i++) {
        if (strncmp(cmd->name, tmp->name, size) != 0) {
            break;
        }

        printf("%" SHELL_COMPLETION_MATCH_FMT, tmp->name);

        if ((i % SHELL_COMPLETION_NR_MATCHES_PER_LINE) == 0) {
            printf("\n");
        }
    }

    if ((i % SHELL_COMPLETION_NR_MATCHES_PER_LINE) != 1) {
        printf("\n");
    }
}

static int
shell_cmd_check_char(char c)
{
    if (((c >= 'a') && (c <= 'z'))
        || ((c >= 'A') && (c <= 'Z'))
        || ((c >= '0') && (c <= '9'))
        || (c == '-')
        || (c == '_')) {
        return 0;
    }

    return EINVAL;
}

static int
shell_cmd_check(const struct shell_cmd *cmd)
{
    unsigned long i;
    int error;

    for (i = 0; cmd->name[i] != '\0'; i++) {
        error = shell_cmd_check_char(cmd->name[i]);

        if (error) {
            return error;
        }
    }

    if (i == 0) {
        return EINVAL;
    }

    return 0;
}

/*
 * The global lock must be acquired before calling this function.
 */
static void
shell_cmd_add_list(struct shell_cmd *cmd)
{
    struct shell_cmd *prev, *next;

    prev = shell_list;

    if ((prev == NULL)
        || (strcmp(cmd->name, prev->name) < 0)) {
        shell_list = cmd;
        cmd->ls_next = prev;
        return;
    }

    for (;;) {
        next = prev->ls_next;

        if ((next == NULL)
            || (strcmp(cmd->name, next->name) < 0)) {
            break;
        }

        prev = next;
    }

    prev->ls_next = cmd;
    cmd->ls_next = next;
}

/*
 * The global lock must be acquired before calling this function.
 */
static int
shell_cmd_add(struct shell_cmd *cmd)
{
    struct shell_bucket *bucket;
    struct shell_cmd *tmp;

    bucket = shell_bucket_get(cmd->name);
    tmp = bucket->cmd;

    if (tmp == NULL) {
        bucket->cmd = cmd;
        goto out;
    }

    for (;;) {
        if (strcmp(cmd->name, tmp->name) == 0) {
            printf("shell: error: %s: shell command name collision", cmd->name);
            return EEXIST;
        }

        if (tmp->ht_next == NULL) {
            break;
        }

        tmp = tmp->ht_next;
    }

    tmp->ht_next = cmd;

out:
    shell_cmd_add_list(cmd);
    return 0;
}

int
shell_cmd_register(struct shell_cmd *cmd)
{
    int error;

    error = shell_cmd_check(cmd);

    if (error) {
        return error;
    }

    shell_cmd_acquire();
    error = shell_cmd_add(cmd);
    shell_cmd_release();

    return error;
}

static inline const char *
shell_line_str(const struct shell_line *line)
{
    return line->str;
}

static inline unsigned long
shell_line_size(const struct shell_line *line)
{
    return line->size;
}

static inline void
shell_line_reset(struct shell_line *line)
{
    line->str[0] = '\0';
    line->size = 0;
}

static inline void
shell_line_copy(struct shell_line *dest, const struct shell_line *src)
{
    strcpy(dest->str, src->str);
    dest->size = src->size;
}

static inline int
shell_line_cmp(const struct shell_line *a, const struct shell_line *b)
{
    return strcmp(a->str, b->str);
}

static int
shell_line_insert(struct shell_line *line, unsigned long index, char c)
{
    unsigned long remaining_chars;

    if (index > line->size) {
        return EINVAL;
    }

    if ((line->size + 1) == sizeof(line->str)) {
        return ENOMEM;
    }

    remaining_chars = line->size - index;

    if (remaining_chars != 0) {
        memmove(&line->str[index + 1], &line->str[index], remaining_chars);
    }

    line->str[index] = c;
    line->size++;
    line->str[line->size] = '\0';
    return 0;
}

static int
shell_line_erase(struct shell_line *line, unsigned long index)
{
    unsigned long remaining_chars;

    if (index >= line->size) {
        return EINVAL;
    }

    remaining_chars = line->size - index - 1;

    if (remaining_chars != 0) {
        memmove(&line->str[index], &line->str[index + 1], remaining_chars);
    }

    line->size--;
    line->str[line->size] = '\0';
    return 0;
}

static struct shell_line *
shell_history_get(unsigned long index)
{
    return &shell_history[index % ARRAY_SIZE(shell_history)];
}

static struct shell_line *
shell_history_get_newest(void)
{
    return shell_history_get(shell_history_newest);
}

static struct shell_line *
shell_history_get_index(void)
{
    return shell_history_get(shell_history_index);
}

static void
shell_history_reset_index(void)
{
    shell_history_index = shell_history_newest;
}

static inline int
shell_history_same_newest(void)
{
    return (shell_history_newest != shell_history_oldest)
           && shell_line_cmp(shell_history_get_newest(),
                             shell_history_get(shell_history_newest - 1)) == 0;
}

static void
shell_history_push(void)
{
    if ((shell_line_size(shell_history_get_newest()) == 0)
        || shell_history_same_newest()) {
        shell_history_reset_index();
        return;
    }

    shell_history_newest++;
    shell_history_reset_index();

    /* Mind integer overflows */
    if ((shell_history_newest - shell_history_oldest)
        >= ARRAY_SIZE(shell_history)) {
        shell_history_oldest = shell_history_newest
                               - ARRAY_SIZE(shell_history) + 1;
    }
}

static void
shell_history_back(void)
{
    if (shell_history_index == shell_history_oldest) {
        return;
    }

    shell_history_index--;
    shell_line_copy(shell_history_get_newest(), shell_history_get_index());
}

static void
shell_history_forward(void)
{
    if (shell_history_index == shell_history_newest) {
        return;
    }

    shell_history_index++;

    if (shell_history_index == shell_history_newest) {
        shell_line_reset(shell_history_get_newest());
    } else {
        shell_line_copy(shell_history_get_newest(), shell_history_get_index());
    }
}

static void
shell_cmd_help(int argc, char *argv[])
{
    const struct shell_cmd *cmd;

    if (argc > 2) {
        argc = 2;
        argv[1] = "help";
    }

    if (argc == 2) {
        cmd = shell_cmd_lookup(argv[1]);

        if (cmd == NULL) {
            printf("shell: help: %s: command not found\n", argv[1]);
            return;
        }

        printf("usage: %s\n%s\n", cmd->usage, cmd->short_desc);

        if (cmd->long_desc != NULL) {
            printf("\n%s\n", cmd->long_desc);
        }

        return;
    }

    shell_cmd_acquire();

    for (cmd = shell_list; cmd != NULL; cmd = cmd->ls_next) {
        printf("%13s  %s\n", cmd->name, cmd->short_desc);
    }

    shell_cmd_release();
}

static void
shell_cmd_history(int argc, char *argv[])
{
    unsigned long i;

    (void)argc;
    (void)argv;

    /* Mind integer overflows */
    for (i = shell_history_oldest; i != shell_history_newest; i++) {
        printf("%6lu  %s\n", i - shell_history_oldest,
               shell_line_str(shell_history_get(i)));
    }
}

static struct shell_cmd shell_default_cmds[] = {
    SHELL_CMD_INITIALIZER("help", shell_cmd_help,
                          "help [command]",
                          "obtain help about shell commands"),
    SHELL_CMD_INITIALIZER("history", shell_cmd_history,
                          "history",
                          "display history list"),
};

static void
shell_prompt(void)
{
    printf("shell> ");
}

static void
shell_reset(void)
{
    shell_line_reset(shell_history_get_newest());
    shell_cursor = 0;
    shell_prompt();
}

static void
shell_erase(void)
{
    struct shell_line *current_line;
    unsigned long remaining_chars;

    current_line = shell_history_get_newest();
    remaining_chars = shell_line_size(current_line);

    while (shell_cursor != remaining_chars) {
        putchar(' ');
        shell_cursor++;
    }

    while (remaining_chars != 0) {
        printf("\b \b");
        remaining_chars--;
    }

    shell_cursor = 0;
}

static void
shell_restore(void)
{
    struct shell_line *current_line;

    current_line = shell_history_get_newest();
    printf("%s", shell_line_str(current_line));
    shell_cursor = shell_line_size(current_line);
}

static int
shell_is_ctrl_char(char c)
{
    return ((c < ' ') || (c >= 0x7f));
}

static void
shell_process_left(void)
{
    if (shell_cursor == 0) {
        return;
    }

    shell_cursor--;
    printf("\e[1D");
}

static int
shell_process_right(void)
{
    if (shell_cursor >= shell_line_size(shell_history_get_newest())) {
        return EAGAIN;
    }

    shell_cursor++;
    printf("\e[1C");
    return 0;
}

static void
shell_process_up(void)
{
    shell_erase();
    shell_history_back();
    shell_restore();
}

static void
shell_process_down(void)
{
    shell_erase();
    shell_history_forward();
    shell_restore();
}

static void
shell_process_backspace(void)
{
    struct shell_line *current_line;
    unsigned long remaining_chars;
    int error;

    current_line = shell_history_get_newest();
    error = shell_line_erase(current_line, shell_cursor - 1);

    if (error) {
        return;
    }

    shell_cursor--;
    printf("\b%s ", shell_line_str(current_line) + shell_cursor);
    remaining_chars = shell_line_size(current_line) - shell_cursor + 1;

    while (remaining_chars != 0) {
        putchar('\b');
        remaining_chars--;
    }
}

static int
shell_process_raw_char(char c)
{
    struct shell_line *current_line;
    unsigned long remaining_chars;
    int error;

    current_line = shell_history_get_newest();
    error = shell_line_insert(current_line, shell_cursor, c);

    if (error) {
        printf("\nshell: line too long\n");
        return error;
    }

    shell_cursor++;

    if (shell_cursor == shell_line_size(current_line)) {
        putchar(c);
        goto out;
    }

    /*
     * This assumes that the backspace character only moves the cursor
     * without erasing characters.
     */
    printf("%s", shell_line_str(current_line) + shell_cursor - 1);
    remaining_chars = shell_line_size(current_line) - shell_cursor;

    while (remaining_chars != 0) {
        putchar('\b');
        remaining_chars--;
    }

out:
    return 0;
}

static int
shell_process_tabulation(void)
{
    const struct shell_cmd *cmd = NULL; /* GCC */
    const char *name, *str, *word;
    unsigned long i, size, cmd_cursor;
    int error;

    shell_cmd_acquire();

    str = shell_line_str(shell_history_get_newest());
    word = shell_find_word(str);
    size = shell_cursor - (word - str);
    cmd_cursor = shell_cursor - size;

    error = shell_cmd_complete(word, &size, &cmd);

    if (error && (error != EAGAIN)) {
        error = 0;
        goto out;
    }

    if (error == EAGAIN) {
        unsigned long cursor;

        cursor = shell_cursor;
        shell_cmd_print_matches(cmd, size);
        shell_prompt();
        shell_restore();

        /* Keep existing arguments as they are */
        while (shell_cursor != cursor) {
            shell_process_left();
        }
    }

    name = shell_cmd_name(cmd);

    while (shell_cursor != cmd_cursor) {
        shell_process_backspace();
    }

    for (i = 0; i < size; i++) {
        error = shell_process_raw_char(name[i]);

        if (error) {
            goto out;
        }
    }

    error = 0;

out:
    shell_cmd_release();
    return error;
}

static void
shell_esc_seq_up(void)
{
    shell_process_up();
}

static void
shell_esc_seq_down(void)
{
    shell_process_down();
}

static void
shell_esc_seq_next(void)
{
    shell_process_right();
}

static void
shell_esc_seq_prev(void)
{
    shell_process_left();
}

static void
shell_esc_seq_home(void)
{
    while (shell_cursor != 0) {
        shell_process_left();
    }
}

static void
shell_esc_seq_del(void)
{
    int error;

    error = shell_process_right();

    if (error) {
        return;
    }

    shell_process_backspace();
}

static void
shell_esc_seq_end(void)
{
    unsigned long size;

    size = shell_line_size(shell_history_get_newest());

    while (shell_cursor < size) {
        shell_process_right();
    }
}

static const struct shell_esc_seq shell_esc_seqs[] = {
    {  "A", shell_esc_seq_up    },
    {  "B", shell_esc_seq_down  },
    {  "C", shell_esc_seq_next  },
    {  "D", shell_esc_seq_prev  },
    {  "H", shell_esc_seq_home  },
    { "1~", shell_esc_seq_home  },
    { "3~", shell_esc_seq_del   },
    {  "F", shell_esc_seq_end   },
    { "4~", shell_esc_seq_end   },
};

static const struct shell_esc_seq *
shell_esc_seq_lookup(const char *str)
{
    unsigned long i;

    for (i = 0; i < ARRAY_SIZE(shell_esc_seqs); i++) {
        if (strcmp(shell_esc_seqs[i].str, str) == 0) {
            return &shell_esc_seqs[i];
        }
    }

    return NULL;
}

/*
 * Process a single escape sequence character.
 *
 * Return the next escape state or 0 if the sequence is complete.
 */
static int
shell_process_esc_sequence(char c)
{
    static char str[SHELL_ESC_SEQ_MAX_SIZE], *ptr = str;

    const struct shell_esc_seq *seq;
    uintptr_t index;

    index = ptr - str;

    if (index >= (ARRAY_SIZE(str) - 1)) {
        printf("shell: escape sequence too long\n");
        goto reset;
    }

    *ptr = c;
    ptr++;
    *ptr = '\0';

    if ((c >= '@') && (c <= '~')) {
        seq = shell_esc_seq_lookup(str);

        if (seq != NULL) {
            seq->fn();
        }

        goto reset;
    }

    return SHELL_ESC_STATE_CSI;

reset:
    ptr = str;
    return 0;
}

static int
shell_process_args(void)
{
    unsigned long i;
    char c, prev;
    int j;

    snprintf(shell_tmp_line, sizeof(shell_tmp_line), "%s",
             shell_line_str(shell_history_get_newest()));

    for (i = 0, j = 0, prev = SHELL_SEPARATOR;
         (c = shell_tmp_line[i]) != '\0';
         i++, prev = c) {
        if (c == SHELL_SEPARATOR) {
            if (prev != SHELL_SEPARATOR) {
                shell_tmp_line[i] = '\0';
            }
        } else {
            if (prev == SHELL_SEPARATOR) {
                shell_argv[j] = &shell_tmp_line[i];
                j++;

                if (j == ARRAY_SIZE(shell_argv)) {
                    printf("shell: too many arguments\n");
                    return EINVAL;
                }

                shell_argv[j] = NULL;
            }
        }
    }

    shell_argc = j;
    return 0;
}

static void
shell_process_line(void)
{
    const struct shell_cmd *cmd;
    int error;

    cmd = NULL;
    error = shell_process_args();

    if (error) {
        goto out;
    }

    if (shell_argc == 0) {
        goto out;
    }

    cmd = shell_cmd_lookup(shell_argv[0]);

    if (cmd == NULL) {
        printf("shell: %s: command not found\n", shell_argv[0]);
        goto out;
    }

out:
    shell_history_push();

    if (cmd != NULL) {
        cmd->fn(shell_argc, shell_argv);
    }
}

/*
 * Process a single control character.
 *
 * Return an error if the caller should reset the current line state.
 */
static int
shell_process_ctrl_char(char c)
{
    switch (c) {
    case SHELL_ERASE_BS:
    case SHELL_ERASE_DEL:
        shell_process_backspace();
        break;
    case '\t':
        return shell_process_tabulation();
    case '\n':
    case '\r':
        putchar('\n');
        shell_process_line();
        return EAGAIN;
    default:
        return 0;
    }

    return 0;
}

static void
shell_run(void *arg)
{
    int c, error, escape;

    (void)arg;

    for (;;) {
        shell_reset();
        escape = 0;

        for (;;) {
            c = getchar();

            if (escape) {
                switch (escape) {
                case SHELL_ESC_STATE_START:
                    /* XXX CSI and SS3 sequence processing is the same */
                    if ((c == '[') || (c == 'O')) {
                        escape = SHELL_ESC_STATE_CSI;
                    } else {
                        escape = 0;
                    }

                    break;
                case SHELL_ESC_STATE_CSI:
                    escape = shell_process_esc_sequence(c);
                    break;
                default:
                    escape = 0;
                }

                error = 0;
            } else if (shell_is_ctrl_char(c)) {
                if (c == '\e') {
                    escape = SHELL_ESC_STATE_START;
                    error = 0;
                } else {
                    error = shell_process_ctrl_char(c);

                    if (error) {
                        break;
                    }
                }
            } else {
                error = shell_process_raw_char(c);
            }

            if (error) {
                break;
            }
        }
    }
}

void
shell_setup(void)
{
    int error;

    mutex_init(&shell_lock);
    SHELL_REGISTER_CMDS(shell_default_cmds);

    error = thread_create(NULL, shell_run, NULL, "shell",
                          SHELL_STACK_SIZE, THREAD_MIN_PRIORITY);

    if (error) {
        panic("shell: unable to create shell thread");
    }
}
