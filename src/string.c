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

#include <stddef.h>
#include <string.h>

#include <lib/macros.h>

void *
memcpy(void *dest, const void *src, size_t n)
{
    const char *src_ptr;
    char *dest_ptr;
    size_t i;

    dest_ptr = dest;
    src_ptr = src;

    for (i = 0; i < n; i++) {
        *dest_ptr = *src_ptr;
        dest_ptr++;
        src_ptr++;
    }

    return dest;
}

void *
memmove(void *dest, const void *src, size_t n)
{
    const char *src_ptr;
    char *dest_ptr;
    size_t i;

    if (dest <= src) {
        dest_ptr = dest;
        src_ptr = src;

        for (i = 0; i < n; i++) {
            *dest_ptr = *src_ptr;
            dest_ptr++;
            src_ptr++;
        }
    } else {
        dest_ptr = dest + n - 1;
        src_ptr = src + n - 1;

        for (i = 0; i < n; i++) {
            *dest_ptr = *src_ptr;
            dest_ptr--;
            src_ptr--;
        }
    }

    return dest;
}

char *
strcpy(char *dest, const char *src)
{
    char *tmp;

    tmp = dest;

    while ((*dest = *src) != '\0') {
        dest++;
        src++;
    }

    return tmp;
}

size_t
strlen(const char *s)
{
    const char *start;

    start = s;

    while (*s != '\0') {
        s++;
    }

    return (s - start);
}

int
strcmp(const char *s1, const char *s2)
{
    char c1, c2;

    while ((c1 = *s1) == (c2 = *s2)) {
        if (c1 == '\0') {
            return 0;
        }

        s1++;
        s2++;
    }

    return (int)c1 - (int)c2;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
    char c1, c2;

    if (unlikely(n == 0)) {
        return 0;
    }

    while ((n != 0) && (c1 = *s1) == (c2 = *s2)) {
        if (c1 == '\0') {
            return 0;
        }

        n--;
        s1++;
        s2++;
    }

    return (int)c1 - (int)c2;
}
