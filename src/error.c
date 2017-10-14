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

#include <stdio.h>
#include <stdlib.h>

#include <lib/macros.h>

#include "error.h"
#include "panic.h"

/*
 * Error message table.
 *
 * This table must be consistent with the enum defined in error.h.
 */
static const char *error_msg_table[] = {
    "success",
    "invalid argument",
    "resource temporarily unavailable",
    "not enough space",
    "input/output error",
    "resource busy",
    "entry exist",
};

const char *
error_str(unsigned int error)
{
    if (error >= ARRAY_SIZE(error_msg_table)) {
        return "invalid error code";
    }

    return error_msg_table[error];
}

void
error_check(int error, const char *prefix)
{
    if (!error) {
        return;
    }

    panic("%s%s%s",
          (prefix == NULL) ? "" : prefix,
          (prefix == NULL) ? "" : ": ",
          error_str(error));
}
