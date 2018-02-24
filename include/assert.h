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

#ifndef ASSERT_H
#define ASSERT_H

#ifdef NDEBUG

/*
 * The assert() macro normally doesn't produce side effects when turned off,
 * but this may result in many "set but not used" warnings. Using sizeof()
 * silences these warnings without producing side effects.
 */
#define assert(expression) ((void)sizeof(expression))

#else /* NDEBUG */

#include <lib/macros.h>
#include <src/panic.h>

/*
 * Panic if the given expression is false.
 */
#define assert(expression)                                          \
MACRO_BEGIN                                                         \
    if (unlikely(!(expression))) {                                  \
        panic("assertion (%s) failed in %s:%d, function %s()",      \
              __QUOTE(expression), __FILE__, __LINE__, __func__);   \
    }                                                               \
MACRO_END

#endif /* NDEBUG */

#endif /* ASSERT_H */
