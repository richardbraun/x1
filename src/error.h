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
 */

#ifndef _ERROR_H
#define _ERROR_H

enum {
    ERROR_INVAL = 1,
    ERROR_AGAIN,
    ERROR_NOMEM,
    ERROR_IO,
    ERROR_BUSY,
    ERROR_EXIST,
};

/*
 * Return the message matching the given error.
 *
 * The returned address points to a statically allocated, read only,
 * null-terminated string literal. The caller must not attempt to use it
 * for anything else than error reporting.
 */
const char * error_str(unsigned int error);

/*
 * If error denotes an actual error (i.e. is not 0), panic, using the given
 * string as a prefix for the error message. A NULL prefix is allowed.
 */
void error_check(int error, const char *prefix);

#endif /* _ERROR_H */
