/*
 * Copyright (c) 2010-2017 Richard Braun.
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <lib/fmt.h>
#include <lib/macros.h>

/*
 * XXX This type is specified by POSIX. Directly declare it for simplicity.
 */
typedef long ssize_t;

/*
 * Size for the temporary number buffer. The minimum base is 8 so 3 bits
 * are consumed per digit. Add one to round up. The conversion algorithm
 * doesn't use the null byte.
 */
#define FMT_MAX_NUM_SIZE (((sizeof(unsigned long long) * CHAR_BIT) / 3) + 1)

/*
 * Special size for fmt_vsnprintf(), used when the buffer size is unknown.
 */
#define FMT_NOLIMIT ((size_t)-1)

/*
 * Formatting flags.
 *
 * FMT_FORMAT_LOWER must be 0x20 as it is OR'd with fmt_digits, eg.
 * '0': 0x30 | 0x20 => 0x30 ('0')
 * 'A': 0x41 | 0x20 => 0x61 ('a')
 */
#define FMT_FORMAT_ALT_FORM     0x0001 /* "Alternate form"                    */
#define FMT_FORMAT_ZERO_PAD     0x0002 /* Zero padding on the left            */
#define FMT_FORMAT_LEFT_JUSTIFY 0x0004 /* Align text on the left              */
#define FMT_FORMAT_BLANK        0x0008 /* Blank space before positive number  */
#define FMT_FORMAT_SIGN         0x0010 /* Always place a sign (either + or -) */
#define FMT_FORMAT_LOWER        0x0020 /* To lowercase (for %x)               */
#define FMT_FORMAT_CONV_SIGNED  0x0040 /* Format specifies signed conversion  */
#define FMT_FORMAT_DISCARD      0x0080 /* Discard output (scanf)              */
#define FMT_FORMAT_CHECK_WIDTH  0x0100 /* Check field width (scanf)           */

enum {
    FMT_MODIFIER_NONE,
    FMT_MODIFIER_CHAR,
    FMT_MODIFIER_SHORT,
    FMT_MODIFIER_LONG,
    FMT_MODIFIER_LONGLONG,
    FMT_MODIFIER_PTR,       /* Used only for %p */
    FMT_MODIFIER_SIZE,
    FMT_MODIFIER_PTRDIFF,
};

enum {
    FMT_SPECIFIER_INVALID,
    FMT_SPECIFIER_INT,
    FMT_SPECIFIER_CHAR,
    FMT_SPECIFIER_STR,
    FMT_SPECIFIER_NRCHARS,
    FMT_SPECIFIER_PERCENT,
};

/*
 * Note that copies of the original va_list object are made, because va_arg()
 * may not reliably be used by different callee functions, and despite the
 * standard explicitely allowing pointers to va_list objects, it's apparently
 * very difficult for implementations to provide and is best avoided.
 */

struct fmt_sprintf_state {
    const char *format;
    va_list ap;
    unsigned int flags;
    int width;
    int precision;
    unsigned int modifier;
    unsigned int specifier;
    unsigned int base;
    char *str;
    char *start;
    char *end;
};

struct fmt_sscanf_state {
    const char *str;
    const char *start;
    const char *format;
    va_list ap;
    unsigned int flags;
    int width;
    int precision;
    unsigned int modifier;
    unsigned int specifier;
    unsigned int base;
    int nr_convs;
};

static const char fmt_digits[] = "0123456789ABCDEF";

static char
fmt_consume(const char **strp)
{
    char c;

    c = **strp;
    (*strp)++;
    return c;
}

static void
fmt_restore(const char **strp)
{
    (*strp)--;
}

static void
fmt_vsnprintf_produce(char **strp, char *end, char c)
{
    if (*strp < end) {
        **strp = c;
    }

    (*strp)++;
}

static bool
fmt_isdigit(char c)
{
    return (c >= '0') && (c <= '9');
}

static bool
fmt_isxdigit(char c)
{
    return fmt_isdigit(c)
           || ((c >= 'a') && (c <= 'f'))
           || ((c >= 'A') && (c <= 'F'));
}

static void
fmt_sprintf_state_init(struct fmt_sprintf_state *state,
                       char *str, size_t size,
                       const char *format, va_list ap)
{
    state->format = format;
    va_copy(state->ap, ap);
    state->str = str;
    state->start = str;

    if (size == 0) {
        state->end = NULL;
    } else if (size == FMT_NOLIMIT) {
        state->end = (char *)-1;
    } else {
        state->end = state->start + size - 1;
    }
}

static int
fmt_sprintf_state_finalize(struct fmt_sprintf_state *state)
{
    va_end(state->ap);

    if (state->str < state->end) {
        *state->str = '\0';
    } else if (state->end != NULL) {
        *state->end = '\0';
    }

    return state->str - state->start;
}

static char
fmt_sprintf_state_consume_format(struct fmt_sprintf_state *state)
{
    return fmt_consume(&state->format);
}

static void
fmt_sprintf_state_restore_format(struct fmt_sprintf_state *state)
{
    fmt_restore(&state->format);
}

static void
fmt_sprintf_state_consume_flags(struct fmt_sprintf_state *state)
{
    bool found;
    char c;

    found = true;
    state->flags = 0;

    do {
        c = fmt_sprintf_state_consume_format(state);

        switch (c) {
        case '#':
            state->flags |= FMT_FORMAT_ALT_FORM;
            break;
        case '0':
            state->flags |= FMT_FORMAT_ZERO_PAD;
            break;
        case '-':
            state->flags |= FMT_FORMAT_LEFT_JUSTIFY;
            break;
        case ' ':
            state->flags |= FMT_FORMAT_BLANK;
            break;
        case '+':
            state->flags |= FMT_FORMAT_SIGN;
            break;
        default:
            found = false;
            break;
        }
    } while (found);

    fmt_sprintf_state_restore_format(state);
}

static void
fmt_sprintf_state_consume_width(struct fmt_sprintf_state *state)
{
    char c;

    c = fmt_sprintf_state_consume_format(state);

    if (fmt_isdigit(c)) {
        state->width = 0;

        do {
            state->width = state->width * 10 + (c - '0');
            c = fmt_sprintf_state_consume_format(state);
        } while (fmt_isdigit(c));

        fmt_sprintf_state_restore_format(state);
    } else if (c == '*') {
        state->width = va_arg(state->ap, int);

        if (state->width < 0) {
            state->flags |= FMT_FORMAT_LEFT_JUSTIFY;
            state->width = -state->width;
        }
    } else {
        state->width = 0;
        fmt_sprintf_state_restore_format(state);
    }
}

static void
fmt_sprintf_state_consume_precision(struct fmt_sprintf_state *state)
{
    char c;

    c = fmt_sprintf_state_consume_format(state);

    if (c == '.') {
        c = fmt_sprintf_state_consume_format(state);

        if (fmt_isdigit(c)) {
            state->precision = 0;

            do {
                state->precision = state->precision * 10 + (c - '0');
                c = fmt_sprintf_state_consume_format(state);
            } while (fmt_isdigit(c));

            fmt_sprintf_state_restore_format(state);
        } else if (c == '*') {
            state->precision = va_arg(state->ap, int);

            if (state->precision < 0) {
                state->precision = 0;
            }
        } else {
            state->precision = 0;
            fmt_sprintf_state_restore_format(state);
        }
    } else {
        /* precision is >= 0 only if explicit */
        state->precision = -1;
        fmt_sprintf_state_restore_format(state);
    }
}

static void
fmt_sprintf_state_consume_modifier(struct fmt_sprintf_state *state)
{
    char c, c2;

    c = fmt_sprintf_state_consume_format(state);

    switch (c) {
    case 'h':
    case 'l':
        c2 = fmt_sprintf_state_consume_format(state);

        if (c == c2) {
            state->modifier = (c == 'h') ? FMT_MODIFIER_CHAR
                                         : FMT_MODIFIER_LONGLONG;
        } else {
            state->modifier = (c == 'h') ? FMT_MODIFIER_SHORT
                                         : FMT_MODIFIER_LONG;
            fmt_sprintf_state_restore_format(state);
        }

        break;
    case 'z':
        state->modifier = FMT_MODIFIER_SIZE;
    case 't':
        state->modifier = FMT_MODIFIER_PTRDIFF;
        break;
    default:
        state->modifier = FMT_MODIFIER_NONE;
        fmt_sprintf_state_restore_format(state);
        break;
    }
}

static void
fmt_sprintf_state_consume_specifier(struct fmt_sprintf_state *state)
{
    char c;

    c = fmt_sprintf_state_consume_format(state);

    switch (c) {
    case 'd':
    case 'i':
        state->flags |= FMT_FORMAT_CONV_SIGNED;
    case 'u':
        state->base = 10;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'o':
        state->base = 8;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'p':
        state->flags |= FMT_FORMAT_ALT_FORM;
        state->modifier = FMT_MODIFIER_PTR;
    case 'x':
        state->flags |= FMT_FORMAT_LOWER;
    case 'X':
        state->base = 16;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'c':
        state->specifier = FMT_SPECIFIER_CHAR;
        break;
    case 's':
        state->specifier = FMT_SPECIFIER_STR;
        break;
    case 'n':
        state->specifier = FMT_SPECIFIER_NRCHARS;
        break;
    case '%':
        state->specifier = FMT_SPECIFIER_PERCENT;
        break;
    default:
        state->specifier = FMT_SPECIFIER_INVALID;
        fmt_sprintf_state_restore_format(state);
        break;
    }
}

static void
fmt_sprintf_state_produce_raw_char(struct fmt_sprintf_state *state, char c)
{
    fmt_vsnprintf_produce(&state->str, state->end, c);
}

static int
fmt_sprintf_state_consume(struct fmt_sprintf_state *state)
{
    char c;

    c = fmt_consume(&state->format);

    if (c == '\0') {
        return EIO;
    }

    if (c != '%') {
        fmt_sprintf_state_produce_raw_char(state, c);
        return EAGAIN;
    }

    fmt_sprintf_state_consume_flags(state);
    fmt_sprintf_state_consume_width(state);
    fmt_sprintf_state_consume_precision(state);
    fmt_sprintf_state_consume_modifier(state);
    fmt_sprintf_state_consume_specifier(state);
    return 0;
}

static void
fmt_sprintf_state_produce_int(struct fmt_sprintf_state *state)
{
    char c, sign, tmp[FMT_MAX_NUM_SIZE];
    unsigned int r, mask, shift;
    unsigned long long n;
    int i;

    switch (state->modifier) {
    case FMT_MODIFIER_CHAR:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = (signed char)va_arg(state->ap, int);
        } else {
            n = (unsigned char)va_arg(state->ap, int);
        }

        break;
    case FMT_MODIFIER_SHORT:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = (short)va_arg(state->ap, int);
        } else {
            n = (unsigned short)va_arg(state->ap, int);
        }

        break;
    case FMT_MODIFIER_LONG:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = va_arg(state->ap, long);
        } else {
            n = va_arg(state->ap, unsigned long);
        }

        break;
    case FMT_MODIFIER_LONGLONG:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = va_arg(state->ap, long long);
        } else {
            n = va_arg(state->ap, unsigned long long);
        }

        break;
    case FMT_MODIFIER_PTR:
        n = (uintptr_t)va_arg(state->ap, void *);
        break;
    case FMT_MODIFIER_SIZE:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = va_arg(state->ap, ssize_t);
        } else {
            n = va_arg(state->ap, size_t);
        }

        break;
    case FMT_MODIFIER_PTRDIFF:
        n = va_arg(state->ap, ptrdiff_t);
        break;
    default:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            n = va_arg(state->ap, int);
        } else {
            n = va_arg(state->ap, unsigned int);
        }

        break;
    }

    if ((state->flags & FMT_FORMAT_LEFT_JUSTIFY) || (state->precision >= 0)) {
        state->flags &= ~FMT_FORMAT_ZERO_PAD;
    }

    sign = '\0';

    if (state->flags & FMT_FORMAT_ALT_FORM) {
        /* '0' for octal */
        state->width--;

        /* '0x' or '0X' for hexadecimal */
        if (state->base == 16) {
            state->width--;
        }
    } else if (state->flags & FMT_FORMAT_CONV_SIGNED) {
        if ((long long)n < 0) {
            sign = '-';
            state->width--;
            n = -(long long)n;
        } else if (state->flags & FMT_FORMAT_SIGN) {
            /* FMT_FORMAT_SIGN must precede FMT_FORMAT_BLANK. */
            sign = '+';
            state->width--;
        } else if (state->flags & FMT_FORMAT_BLANK) {
            sign = ' ';
            state->width--;
        }
    }

    /* Conversion, in reverse order */

    i = 0;

    if (n == 0) {
        if (state->precision != 0) {
            tmp[i] = '0';
            i++;
        }
    } else if (state->base == 10) {
        /*
         * Try to avoid 64 bits operations if the processor doesn't
         * support them. Note that even when using modulus and
         * division operators close to each other, the compiler may
         * forge two functions calls to compute the quotient and the
         * remainder, whereas processor instructions are generally
         * correctly used once, giving both results at once, through
         * plain or reciprocal division.
         */
#ifndef __LP64__
        if (state->modifier == FMT_MODIFIER_LONGLONG) {
#endif /* __LP64__ */
            do {
                r = n % 10;
                n /= 10;
                tmp[i] = fmt_digits[r];
                i++;
            } while (n != 0);
#ifndef __LP64__
        } else {
            unsigned long m;

            m = (unsigned long)n;

            do {
                r = m % 10;
                m /= 10;
                tmp[i] = fmt_digits[r];
                i++;
            } while (m != 0);
        }
#endif /* __LP64__ */
    } else {
        mask = state->base - 1;
        shift = (state->base == 8) ? 3 : 4;

        do {
            r = n & mask;
            n >>= shift;
            tmp[i] = fmt_digits[r] | (state->flags & FMT_FORMAT_LOWER);
            i++;
        } while (n != 0);
    }

    if (i > state->precision) {
        state->precision = i;
    }

    state->width -= state->precision;

    if (!(state->flags & (FMT_FORMAT_LEFT_JUSTIFY | FMT_FORMAT_ZERO_PAD))) {
        while (state->width > 0) {
            state->width--;
            fmt_sprintf_state_produce_raw_char(state, ' ');
        }

        state->width--;
    }

    if (state->flags & FMT_FORMAT_ALT_FORM) {
        fmt_sprintf_state_produce_raw_char(state, '0');

        if (state->base == 16) {
            c = 'X' | (state->flags & FMT_FORMAT_LOWER);
            fmt_sprintf_state_produce_raw_char(state, c);
        }
    } else if (sign != '\0') {
        fmt_sprintf_state_produce_raw_char(state, sign);
    }

    if (!(state->flags & FMT_FORMAT_LEFT_JUSTIFY)) {
        c = (state->flags & FMT_FORMAT_ZERO_PAD) ? '0' : ' ';

        while (state->width > 0) {
            state->width--;
            fmt_sprintf_state_produce_raw_char(state, c);
        }

        state->width--;
    }

    while (i < state->precision) {
        state->precision--;
        fmt_sprintf_state_produce_raw_char(state, '0');
    }

    state->precision--;

    while (i > 0) {
        i--;
        fmt_sprintf_state_produce_raw_char(state, tmp[i]);
    }

    while (state->width > 0) {
        state->width--;
        fmt_sprintf_state_produce_raw_char(state, ' ');
    }

    state->width--;
}

static void
fmt_sprintf_state_produce_char(struct fmt_sprintf_state *state)
{
    char c;

    c = va_arg(state->ap, int);

    if (!(state->flags & FMT_FORMAT_LEFT_JUSTIFY)) {
        for (;;) {
            state->width--;

            if (state->width <= 0) {
                break;
            }

            fmt_sprintf_state_produce_raw_char(state, ' ');
        }
    }

    fmt_sprintf_state_produce_raw_char(state, c);

    for (;;) {
        state->width--;

        if (state->width <= 0) {
            break;
        }

        fmt_sprintf_state_produce_raw_char(state, ' ');
    }
}

static void
fmt_sprintf_state_produce_str(struct fmt_sprintf_state *state)
{
    int i, len;
    char *s;

    s = va_arg(state->ap, char *);

    if (s == NULL) {
        s = "(null)";
    }

    len = 0;

    for (len = 0; s[len] != '\0'; len++) {
        if (len == state->precision) {
            break;
        }
    }

    if (!(state->flags & FMT_FORMAT_LEFT_JUSTIFY)) {
        while (len < state->width) {
            state->width--;
            fmt_sprintf_state_produce_raw_char(state, ' ');
        }
    }

    for (i = 0; i < len; i++) {
        fmt_sprintf_state_produce_raw_char(state, *s);
        s++;
    }

    while (len < state->width) {
        state->width--;
        fmt_sprintf_state_produce_raw_char(state, ' ');
    }
}

static void
fmt_sprintf_state_produce_nrchars(struct fmt_sprintf_state *state)
{
    if (state->modifier == FMT_MODIFIER_CHAR) {
        signed char *ptr = va_arg(state->ap, signed char *);
        *ptr = state->str - state->start;
    } else if (state->modifier == FMT_MODIFIER_SHORT) {
        short *ptr = va_arg(state->ap, short *);
        *ptr = state->str - state->start;
    } else if (state->modifier == FMT_MODIFIER_LONG) {
        long *ptr = va_arg(state->ap, long *);
        *ptr = state->str - state->start;
    } else if (state->modifier == FMT_MODIFIER_LONGLONG) {
        long long *ptr = va_arg(state->ap, long long *);
        *ptr = state->str - state->start;
    } else if (state->modifier == FMT_MODIFIER_SIZE) {
        ssize_t *ptr = va_arg(state->ap, ssize_t *);
        *ptr = state->str - state->start;
    } else if (state->modifier == FMT_MODIFIER_PTRDIFF) {
        ptrdiff_t *ptr = va_arg(state->ap, ptrdiff_t *);
        *ptr = state->str - state->start;
    } else {
        int *ptr = va_arg(state->ap, int *);
        *ptr = state->str - state->start;
    }
}

static void
fmt_sprintf_state_produce(struct fmt_sprintf_state *state)
{
    switch (state->specifier) {
    case FMT_SPECIFIER_INT:
        fmt_sprintf_state_produce_int(state);
        break;
    case FMT_SPECIFIER_CHAR:
        fmt_sprintf_state_produce_char(state);
        break;
    case FMT_SPECIFIER_STR:
        fmt_sprintf_state_produce_str(state);
        break;
    case FMT_SPECIFIER_NRCHARS:
        fmt_sprintf_state_produce_nrchars(state);
        break;
    case FMT_SPECIFIER_PERCENT:
    case FMT_SPECIFIER_INVALID:
        fmt_sprintf_state_produce_raw_char(state, '%');
        break;
    }
}

int
fmt_sprintf(char *str, const char *format, ...)
{
    va_list ap;
    int length;

    va_start(ap, format);
    length = fmt_vsprintf(str, format, ap);
    va_end(ap);

    return length;
}

int
fmt_vsprintf(char *str, const char *format, va_list ap)
{
    return fmt_vsnprintf(str, FMT_NOLIMIT, format, ap);
}

int
fmt_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int length;

    va_start(ap, format);
    length = fmt_vsnprintf(str, size, format, ap);
    va_end(ap);

    return length;
}

int
fmt_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    struct fmt_sprintf_state state;
    int error;

    fmt_sprintf_state_init(&state, str, size, format, ap);

    for (;;) {
        error = fmt_sprintf_state_consume(&state);

        if (error == EAGAIN) {
            continue;
        } else if (error) {
            break;
        }

        fmt_sprintf_state_produce(&state);
    }

    return fmt_sprintf_state_finalize(&state);
}

static char
fmt_atoi(char c)
{
    assert(fmt_isxdigit(c));

    if (fmt_isdigit(c)) {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    } else {
        return 10 + (c - 'A');
    }
}

static bool
fmt_isspace(char c)
{
    if (c == ' ') {
        return true;
    }

    if ((c >= '\t') && (c <= '\f')) {
        return true;
    }

    return false;
}

static void
fmt_skip(const char **strp)
{
    while (fmt_isspace(**strp)) {
        (*strp)++;
    }
}

static void
fmt_sscanf_state_init(struct fmt_sscanf_state *state, const char *str,
                      const char *format, va_list ap)
{
    state->str = str;
    state->start = str;
    state->format = format;
    state->flags = 0;
    state->width = 0;
    va_copy(state->ap, ap);
    state->nr_convs = 0;
}

static int
fmt_sscanf_state_finalize(struct fmt_sscanf_state *state)
{
    va_end(state->ap);
    return state->nr_convs;
}

static void
fmt_sscanf_state_report_conv(struct fmt_sscanf_state *state)
{
    if (state->nr_convs == EOF) {
        state->nr_convs = 1;
        return;
    }

    state->nr_convs++;
}

static void
fmt_sscanf_state_report_error(struct fmt_sscanf_state *state)
{
    if (state->nr_convs != 0) {
        return;
    }

    state->nr_convs = EOF;
}

static void
fmt_sscanf_state_skip_space(struct fmt_sscanf_state *state)
{
    fmt_skip(&state->str);
}

static char
fmt_sscanf_state_consume_string(struct fmt_sscanf_state *state)
{
    char c;

    c = fmt_consume(&state->str);

    if (state->flags & FMT_FORMAT_CHECK_WIDTH) {
        if (state->width == 0) {
            c = EOF;
        } else {
            state->width--;
        }
    }

    return c;
}

static void
fmt_sscanf_state_restore_string(struct fmt_sscanf_state *state)
{
    fmt_restore(&state->str);
}

static char
fmt_sscanf_state_consume_format(struct fmt_sscanf_state *state)
{
    return fmt_consume(&state->format);
}

static void
fmt_sscanf_state_restore_format(struct fmt_sscanf_state *state)
{
    fmt_restore(&state->format);
}

static void
fmt_sscanf_state_consume_flags(struct fmt_sscanf_state *state)
{
    bool found;
    char c;

    found = true;
    state->flags = 0;

    do {
        c = fmt_sscanf_state_consume_format(state);

        switch (c) {
        case '*':
            state->flags |= FMT_FORMAT_DISCARD;
            break;
        default:
            found = false;
            break;
        }
    } while (found);

    fmt_sscanf_state_restore_format(state);
}

static void
fmt_sscanf_state_consume_width(struct fmt_sscanf_state *state)
{
    char c;

    state->width = 0;

    for (;;) {
        c = fmt_sscanf_state_consume_format(state);

        if (!fmt_isdigit(c)) {
            break;
        }

        state->width = state->width * 10 + (c - '0');
    }

    if (state->width != 0) {
        state->flags |= FMT_FORMAT_CHECK_WIDTH;
    }

    fmt_sscanf_state_restore_format(state);
}

static void
fmt_sscanf_state_consume_modifier(struct fmt_sscanf_state *state)
{
    char c, c2;

    c = fmt_sscanf_state_consume_format(state);

    switch (c) {
    case 'h':
    case 'l':
        c2 = fmt_sscanf_state_consume_format(state);

        if (c == c2) {
            state->modifier = (c == 'h') ? FMT_MODIFIER_CHAR
                                         : FMT_MODIFIER_LONGLONG;
        } else {
            state->modifier = (c == 'h') ? FMT_MODIFIER_SHORT
                                         : FMT_MODIFIER_LONG;
            fmt_sscanf_state_restore_format(state);
        }

        break;
    case 'z':
        state->modifier = FMT_MODIFIER_SIZE;
        break;
    case 't':
        state->modifier = FMT_MODIFIER_PTRDIFF;
        break;
    default:
        state->modifier = FMT_MODIFIER_NONE;
        fmt_sscanf_state_restore_format(state);
        break;
    }
}

static void
fmt_sscanf_state_consume_specifier(struct fmt_sscanf_state *state)
{
    char c;

    c = fmt_sscanf_state_consume_format(state);

    switch (c) {
    case 'i':
        state->base = 0;
        state->flags |= FMT_FORMAT_CONV_SIGNED;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'd':
        state->flags |= FMT_FORMAT_CONV_SIGNED;
    case 'u':
        state->base = 10;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'o':
        state->base = 8;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'p':
        state->modifier = FMT_MODIFIER_PTR;
    case 'x':
    case 'X':
        state->base = 16;
        state->specifier = FMT_SPECIFIER_INT;
        break;
    case 'c':
        state->specifier = FMT_SPECIFIER_CHAR;
        break;
    case 's':
        state->specifier = FMT_SPECIFIER_STR;
        break;
    case 'n':
        state->specifier = FMT_SPECIFIER_NRCHARS;
        break;
    case '%':
        state->specifier = FMT_SPECIFIER_PERCENT;
        break;
    default:
        state->specifier = FMT_SPECIFIER_INVALID;
        fmt_sscanf_state_restore_format(state);
        break;
    }
}

static int
fmt_sscanf_state_discard_char(struct fmt_sscanf_state *state, char c)
{
    char c2;

    if (fmt_isspace(c)) {
        fmt_sscanf_state_skip_space(state);
        return 0;
    }

    c2 = fmt_sscanf_state_consume_string(state);

    if (c != c2) {
        if ((c2 == '\0') && (state->nr_convs == 0)) {
            state->nr_convs = EOF;
        }

        return EINVAL;
    }

    return 0;
}

static int
fmt_sscanf_state_consume(struct fmt_sscanf_state *state)
{
    int error;
    char c;

    state->flags = 0;

    c = fmt_consume(&state->format);

    if (c == '\0') {
        return EIO;
    }

    if (c != '%') {
        error = fmt_sscanf_state_discard_char(state, c);

        if (error) {
            return error;
        }

        return EAGAIN;
    }

    fmt_sscanf_state_consume_flags(state);
    fmt_sscanf_state_consume_width(state);
    fmt_sscanf_state_consume_modifier(state);
    fmt_sscanf_state_consume_specifier(state);
    return 0;
}

static int
fmt_sscanf_state_produce_int(struct fmt_sscanf_state *state)
{
    unsigned long long n, m, tmp;
    char c, buf[FMT_MAX_NUM_SIZE];
    bool negative;
    size_t i;

    negative = 0;

    fmt_sscanf_state_skip_space(state);
    c = fmt_sscanf_state_consume_string(state);

    if (c == '-') {
        negative = true;
        c = fmt_sscanf_state_consume_string(state);
    }

    if (c == '0') {
        c = fmt_sscanf_state_consume_string(state);

        if ((c == 'x') || (c == 'X')) {
            if (state->base == 0) {
                state->base = 16;
            }

            if (state->base == 16) {
                c = fmt_sscanf_state_consume_string(state);
            } else {
                fmt_sscanf_state_restore_string(state);
                c = '0';
            }
        } else {
            if (state->base == 0) {
                state->base = 8;
            }

            if (state->base != 8) {
                fmt_sscanf_state_restore_string(state);
                c = '0';
            }
        }
    }

    i = 0;

    while (c != '\0') {
        if (state->base == 8) {
            if (!((c >= '0') && (c <= '7'))) {
                break;
            }
        } else if (state->base == 16) {
            if (!fmt_isxdigit(c)) {
                break;
            }
        } else {
            if (!fmt_isdigit(c)) {
                break;
            }
        }

        /* XXX Standard sscanf provides no way to cleanly handle overflows */
        if (i < (ARRAY_SIZE(buf) - 1)) {
            buf[i] = c;
        } else if (i == (ARRAY_SIZE(buf) - 1)) {
            strcpy(buf, "1");
            negative = true;
        }

        i++;
        c = fmt_sscanf_state_consume_string(state);
    }

    fmt_sscanf_state_restore_string(state);

    if (state->flags & FMT_FORMAT_DISCARD) {
        return 0;
    }

    if (i == 0) {
        if (c == '\0') {
            fmt_sscanf_state_report_error(state);
            return EINVAL;
        }

        buf[0] = '0';
        i = 1;
    }

    if (i < ARRAY_SIZE(buf)) {
        buf[i] = '\0';
        i--;
    } else {
        i = strlen(buf) - 1;
    }

    n = 0;

#ifndef __LP64__
    if (state->modifier == FMT_MODIFIER_LONGLONG) {
#endif /* __LP64__ */
        m = 1;
        tmp = 0;

        while (&buf[i] >= buf) {
            tmp += fmt_atoi(buf[i]) * m;

            if (tmp < n) {
                n = 1;
                negative = true;
                break;
            }

            n = tmp;
            m *= state->base;
            i--;
        }
#ifndef __LP64__
    } else {
        unsigned long _n, _m, _tmp;

        _n = 0;
        _m = 1;
        _tmp = 0;

        while (&buf[i] >= buf) {
            _tmp += fmt_atoi(buf[i]) * _m;

            if (_tmp < _n) {
                _n = 1;
                negative = true;
                break;
            }

            _n = _tmp;
            _m *= state->base;
            i--;
        }

        n = _n;
    }
#endif /* __LP64__ */

    if (negative) {
        n = -n;
    }

    switch (state->modifier) {
    case FMT_MODIFIER_CHAR:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            *va_arg(state->ap, char *) = n;
        } else {
            *va_arg(state->ap, unsigned char *) = n;
        }

        break;
    case FMT_MODIFIER_SHORT:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            *va_arg(state->ap, short *) = n;
        } else {
            *va_arg(state->ap, unsigned short *) = n;
        }

        break;
    case FMT_MODIFIER_LONG:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            *va_arg(state->ap, long *) = n;
        } else {
            *va_arg(state->ap, unsigned long *) = n;
        }

        break;
    case FMT_MODIFIER_LONGLONG:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            *va_arg(state->ap, long long *) = n;
        } else {
            *va_arg(state->ap, unsigned long long *) = n;
        }

        break;
    case FMT_MODIFIER_PTR:
        *va_arg(state->ap, uintptr_t *) = n;
        break;
    case FMT_MODIFIER_SIZE:
        *va_arg(state->ap, size_t *) = n;
        break;
    case FMT_MODIFIER_PTRDIFF:
        *va_arg(state->ap, ptrdiff_t *) = n;
        break;
    default:
        if (state->flags & FMT_FORMAT_CONV_SIGNED) {
            *va_arg(state->ap, int *) = n;
        } else {
            *va_arg(state->ap, unsigned int *) = n;
        }
    }

    fmt_sscanf_state_report_conv(state);
    return 0;
}

static int
fmt_sscanf_state_produce_char(struct fmt_sscanf_state *state)
{
    char c, *dest;
    int i, width;

    if (state->flags & FMT_FORMAT_DISCARD) {
        dest = NULL;
    } else {
        dest = va_arg(state->ap, char *);
    }

    if (state->flags & FMT_FORMAT_CHECK_WIDTH) {
        width = state->width;
    } else {
        width = 1;
    }

    for (i = 0; i < width; i++) {
        c = fmt_sscanf_state_consume_string(state);

        if ((c == '\0') || (c == EOF)) {
            break;
        }

        if (dest != NULL) {
            *dest = c;
            dest++;
        }
    }

    if (i < width) {
        fmt_sscanf_state_restore_string(state);
    }

    if ((dest != NULL) && (i != 0)) {
        fmt_sscanf_state_report_conv(state);
    }

    return 0;
}

static int
fmt_sscanf_state_produce_str(struct fmt_sscanf_state *state)
{
    const char *orig;
    char c, *dest;

    orig = state->str;

    fmt_sscanf_state_skip_space(state);

    if (state->flags & FMT_FORMAT_DISCARD) {
        dest = NULL;
    } else {
        dest = va_arg(state->ap, char *);
    }

    for (;;) {
        c = fmt_sscanf_state_consume_string(state);

        if ((c == '\0') || (c == ' ') || (c == EOF)) {
            break;
        }

        if (dest != NULL) {
            *dest = c;
            dest++;
        }
    }

    fmt_sscanf_state_restore_string(state);

    if (state->str == orig) {
        fmt_sscanf_state_report_error(state);
        return EINVAL;
    }

    if (dest != NULL) {
        *dest = '\0';
        fmt_sscanf_state_report_conv(state);
    }

    return 0;
}

static int
fmt_sscanf_state_produce_nrchars(struct fmt_sscanf_state *state)
{
    *va_arg(state->ap, int *) = state->str - state->start;
    return 0;
}

static int
fmt_sscanf_state_produce(struct fmt_sscanf_state *state)
{
    switch (state->specifier) {
    case FMT_SPECIFIER_INT:
        return fmt_sscanf_state_produce_int(state);
    case FMT_SPECIFIER_CHAR:
        return fmt_sscanf_state_produce_char(state);
    case FMT_SPECIFIER_STR:
        return fmt_sscanf_state_produce_str(state);
    case FMT_SPECIFIER_NRCHARS:
        return fmt_sscanf_state_produce_nrchars(state);
    case FMT_SPECIFIER_PERCENT:
        fmt_sscanf_state_skip_space(state);
        return fmt_sscanf_state_discard_char(state, '%');
    default:
        fmt_sscanf_state_report_error(state);
        return EINVAL;
    }
}

int
fmt_sscanf(const char *str, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = fmt_vsscanf(str, format, ap);
    va_end(ap);

    return ret;
}

int
fmt_vsscanf(const char *str, const char *format, va_list ap)
{
    struct fmt_sscanf_state state;
    int error;

    fmt_sscanf_state_init(&state, str, format, ap);

    for (;;) {
        error = fmt_sscanf_state_consume(&state);

        if (error == EAGAIN) {
            continue;
        } else if (error) {
            break;
        }

        error = fmt_sscanf_state_produce(&state);

        if (error) {
            break;
        }
    }

    return fmt_sscanf_state_finalize(&state);
}
