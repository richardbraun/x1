/*
 * Copyright (c) 2009-2018 Richard Braun.
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
 * Helper macros.
 */

#ifndef _MACROS_H
#define _MACROS_H

#if !defined(__GNUC__) || (__GNUC__ < 4)
#error "GCC 4+ required"
#endif

#ifndef __ASSEMBLER__
#include <stddef.h>
#endif

#define MACRO_BEGIN         ({
#define MACRO_END           })

#define __QUOTE(x)          #x
#define QUOTE(x)            __QUOTE(x)

#define STRLEN(x)           (sizeof(x) - 1)
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof((x)[0]))

#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

#define DIV_CEIL(n, d)      (((n) + (d) - 1) / (d))

#define P2ALIGNED(x, a)     (((x) & ((a) - 1)) == 0)
#define ISP2(x)             P2ALIGNED(x, x)
#define P2ALIGN(x, a)       ((x) & -(a))        /* decreases if not aligned */
#define P2ROUND(x, a)       (-(-(x) & -(a)))    /* increases if not aligned */
#define P2END(x, a)         (-(~(x) & -(a)))    /* always increases */

#define structof(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define likely(expr)        __builtin_expect(!!(expr), 1)
#define unlikely(expr)      __builtin_expect(!!(expr), 0)

#define barrier()           asm volatile("" : : : "memory")

/*
 * The following macros may be provided by the C environment.
 */

#ifndef __noinline
#define __noinline          __attribute__((noinline))
#endif

#ifndef __aligned
#define __aligned(x)        __attribute__((aligned(x)))
#endif

#ifndef __always_inline
#define __always_inline     inline __attribute__((always_inline))
#endif

#ifndef __section
#define __section(x)        __attribute__((section(x)))
#endif

#ifndef __packed
#define __packed            __attribute__((packed))
#endif

#ifndef __unused
#define __unused            __attribute__((unused))
#endif

#ifndef __used
#define __used              __attribute__((used))
#endif

#ifndef __fallthrough
#if __GNUC__ >= 7
#define __fallthrough       __attribute__((fallthrough))
#else /* __GNUC__ >= 7 */
#define __fallthrough
#endif /* __GNUC__ >= 7 */
#endif

#endif /* _MACROS_H */
