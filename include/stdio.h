/*
 * Copyright (c) 2017 Richard Braun.
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
 *
 * Subset of the standard C stdio interface.
 */

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

#include <lib/fmt.h>

/*
 * Keep in mind that #define really means "textually replace". As a result,
 * any expression that may cause errors because of operator precedance should
 * be enclosed in parentheses.
 */
#define EOF (-1)

void putchar(unsigned char c);
int getchar(void);

int printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

int vprintf(const char *format, va_list ap)
    __attribute__((format(printf, 1, 0)));

#define sprintf     fmt_sprintf
#define vsprintf    fmt_vsprintf
#define snprintf    fmt_snprintf
#define vsnprintf   fmt_vsnprintf
#define sscanf      fmt_sscanf
#define vsscanf     fmt_vsscanf

#endif /* STDIO_H */
