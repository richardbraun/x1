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
 *
 *
 * Formatted string functions.
 *
 * The functions provided by this module implement a subset of the standard
 * sprintf- and sscanf-like functions.
 *
 * sprintf:
 *  - flags: # 0 - ' ' (space) +
 *  - field width is supported
 *  - precision is supported
 *
 * sscanf:
 *  - flags: *
 *  - field width is supported
 *
 * common:
 *  - modifiers: hh h l ll z t
 *  - specifiers: d i o u x X c s p n %
 */

#ifndef FMT_H
#define FMT_H

#include <stdarg.h>
#include <stddef.h>

int fmt_sprintf(char *str, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

int fmt_vsprintf(char *str, const char *format, va_list ap)
    __attribute__((format(printf, 2, 0)));

int fmt_snprintf(char *str, size_t size, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

int fmt_vsnprintf(char *str, size_t size, const char *format, va_list ap)
    __attribute__((format(printf, 3, 0)));

int fmt_sscanf(const char *str, const char *format, ...)
    __attribute__((format(scanf, 2, 3)));

int fmt_vsscanf(const char *str, const char *format, va_list ap)
    __attribute__((format(scanf, 2, 0)));

#endif /* FMT_H */
