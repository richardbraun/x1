/*
 * Copyright (c) 2015-2017 Richard Braun.
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <lib/cbuf.h>
#include <lib/macros.h>

#include <src/error.h>

/* Negative close to 0 so that an overflow occurs early */
#define CBUF_INIT_INDEX ((size_t)-500)

void
cbuf_init(struct cbuf *cbuf, void *buf, size_t capacity)
{
    assert(ISP2(capacity));

    cbuf->buf = buf;
    cbuf->capacity = capacity;
    cbuf->start = CBUF_INIT_INDEX;
    cbuf->end = cbuf->start;
}

static size_t
cbuf_index(const struct cbuf *cbuf, size_t abs_index)
{
    return abs_index & (cbuf->capacity - 1);
}

static void
cbuf_update_start(struct cbuf *cbuf)
{
    /* Mind integer overflows */
    if (cbuf_size(cbuf) > cbuf->capacity) {
        cbuf->start = cbuf->end - cbuf->capacity;
    }
}

int
cbuf_push(struct cbuf *cbuf, const void *buf, size_t size, bool erase)
{
    size_t free_size;

    if (!erase) {
        free_size = cbuf_capacity(cbuf) - cbuf_size(cbuf);

        if (size > free_size) {
            return ERROR_AGAIN;
        }
    }

    return cbuf_write(cbuf, cbuf_end(cbuf), buf, size);
}

int
cbuf_pop(struct cbuf *cbuf, void *buf, size_t *sizep)
{
    __unused int error;

    if (cbuf_size(cbuf) == 0) {
        return ERROR_AGAIN;
    }

    error = cbuf_read(cbuf, cbuf_start(cbuf), buf, sizep);
    assert(!error);
    cbuf->start += *sizep;
    return 0;
}

int
cbuf_pushb(struct cbuf *cbuf, uint8_t byte, bool erase)
{
    size_t free_size;

    if (!erase) {
        free_size = cbuf_capacity(cbuf) - cbuf_size(cbuf);

        if (free_size == 0) {
            return ERROR_AGAIN;
        }
    }

    cbuf->buf[cbuf_index(cbuf, cbuf->end)] = byte;
    cbuf->end++;
    cbuf_update_start(cbuf);
    return 0;
}

int
cbuf_popb(struct cbuf *cbuf, void *bytep)
{
    uint8_t *ptr;

    if (cbuf_size(cbuf) == 0) {
        return ERROR_AGAIN;
    }

    ptr = bytep;
    *ptr = cbuf->buf[cbuf_index(cbuf, cbuf->start)];
    cbuf->start++;
    return 0;
}

int
cbuf_write(struct cbuf *cbuf, size_t index, const void *buf, size_t size)
{
    uint8_t *start, *end, *buf_end;
    size_t new_end, skip;

    if (!cbuf_range_valid(cbuf, index, cbuf->end)) {
        return ERROR_INVAL;
    }

    new_end = index + size;

    if (!cbuf_range_valid(cbuf, cbuf->start, new_end)) {
        cbuf->end = new_end;

        if (size > cbuf_capacity(cbuf)) {
            skip = size - cbuf_capacity(cbuf);
            buf += skip;
            index += skip;
            size = cbuf_capacity(cbuf);
        }
    }

    start = &cbuf->buf[cbuf_index(cbuf, index)];
    end = start + size;
    buf_end = cbuf->buf + cbuf->capacity;

    if ((end <= cbuf->buf) || (end > buf_end)) {
        skip = buf_end - start;
        memcpy(start, buf, skip);
        buf += skip;
        start = cbuf->buf;
        size -= skip;
    }

    memcpy(start, buf, size);
    cbuf_update_start(cbuf);
    return 0;
}

int
cbuf_read(const struct cbuf *cbuf, size_t index, void *buf, size_t *sizep)
{
    const uint8_t *start, *end, *buf_end;
    size_t size;

    /* At least one byte must be available */
    if (!cbuf_range_valid(cbuf, index, index + 1)) {
        return ERROR_INVAL;
    }

    size = cbuf->end - index;

    if (*sizep > size) {
        *sizep = size;
    }

    start = &cbuf->buf[cbuf_index(cbuf, index)];
    end = start + *sizep;
    buf_end = cbuf->buf + cbuf->capacity;

    if ((end > cbuf->buf) && (end <= buf_end)) {
        size = *sizep;
    } else {
        size = buf_end - start;
        memcpy(buf, start, size);
        buf += size;
        start = cbuf->buf;
        size = *sizep - size;
    }

    memcpy(buf, start, size);
    return 0;
}
