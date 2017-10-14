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
 * Hash functions for integers and strings.
 *
 * Integer hashing follows Thomas Wang's paper about his 32/64-bits mix
 * functions :
 * - https://gist.github.com/badboy/6267743
 *
 * String hashing uses a variant of the djb2 algorithm with k=31, as in
 * the implementation of the hashCode() method of the Java String class :
 * - http://www.javamex.com/tutorials/collections/hash_function_technical.shtml
 *
 * Note that this algorithm isn't suitable to obtain usable 64-bits hashes
 * and is expected to only serve as an array index producer.
 *
 * These functions all have a bits parameter that indicates the number of
 * relevant bits the caller is interested in. When returning a hash, its
 * value must be truncated so that it can fit in the requested bit size.
 * It can be used by the implementation to select high or low bits, depending
 * on their relative randomness. To get complete, unmasked hashes, use the
 * HASH_ALLBITS macro.
 */

#ifndef _HASH_H
#define _HASH_H

#include <assert.h>
#include <stdint.h>

#ifdef __LP64__
#define HASH_ALLBITS 64
#define hash_long(n, bits) hash_int64(n, bits)
#else /* __LP64__ */
#define HASH_ALLBITS 32
#define hash_long(n, bits) hash_int32(n, bits)
#endif

static inline uint32_t
hash_int32(uint32_t n, unsigned int bits)
{
    uint32_t hash;

    hash = n;
    hash = ~hash + (hash << 15);
    hash ^= (hash >> 12);
    hash += (hash << 2);
    hash ^= (hash >> 4);
    hash += (hash << 3) + (hash << 11);
    hash ^= (hash >> 16);

    return hash >> (32 - bits);
}

static inline uint64_t
hash_int64(uint64_t n, unsigned int bits)
{
    uint64_t hash;

    hash = n;
    hash = ~hash + (hash << 21);
    hash ^= (hash >> 24);
    hash += (hash << 3) + (hash << 8);
    hash ^= (hash >> 14);
    hash += (hash << 2) + (hash << 4);
    hash ^= (hash >> 28);
    hash += (hash << 31);

    return hash >> (64 - bits);
}

static inline uintptr_t
hash_ptr(const void *ptr, unsigned int bits)
{
    if (sizeof(uintptr_t) == 8) {
        return hash_int64((uintptr_t)ptr, bits);
    } else {
        return hash_int32((uintptr_t)ptr, bits);
    }
}

static inline unsigned long
hash_str(const char *str, unsigned int bits)
{
    unsigned long hash;
    char c;

    for (hash = 0; /* no condition */; str++) {
        c = *str;

        if (c == '\0') {
            break;
        }

        hash = ((hash << 5) - hash) + c;
    }

    return hash & ((1 << bits) - 1);
}

#endif /* _HASH_H */
