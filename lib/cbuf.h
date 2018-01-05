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
 *
 *
 * Circular byte buffer.
 */

#ifndef _CBUF_H
#define _CBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Circular buffer descriptor.
 *
 * The buffer capacity must be a power-of-two. Indexes are absolute values
 * which can overflow. Their difference cannot exceed the capacity.
 */
struct cbuf {
    uint8_t *buf;
    size_t capacity;
    size_t start;
    size_t end;
};

static inline size_t
cbuf_capacity(const struct cbuf *cbuf)
{
    return cbuf->capacity;
}

static inline size_t
cbuf_start(const struct cbuf *cbuf)
{
    return cbuf->start;
}

static inline size_t
cbuf_end(const struct cbuf *cbuf)
{
    return cbuf->end;
}

static inline size_t
cbuf_size(const struct cbuf *cbuf)
{
    return cbuf->end - cbuf->start;
}

static inline void
cbuf_clear(struct cbuf *cbuf)
{
    cbuf->start = cbuf->end;
}

static inline bool
cbuf_range_valid(const struct cbuf *cbuf, size_t start, size_t end)
{
    return (((end - start) <= cbuf_size(cbuf))
            && ((start - cbuf->start) <= cbuf_size(cbuf))
            && ((cbuf->end - end) <= cbuf_size(cbuf)));
}

/*
 * Initialize a circular buffer.
 *
 * The descriptor is set to use the given buffer for storage. Capacity
 * must be a power-of-two.
 */
void cbuf_init(struct cbuf *cbuf, void *buf, size_t capacity);

/*
 * Push data to a circular buffer.
 *
 * If the function isn't allowed to erase old data and the circular buffer
 * doesn't have enough unused bytes for the new data, EAGAIN is returned.
 */
int cbuf_push(struct cbuf *cbuf, const void *buf, size_t size, bool erase);

/*
 * Pop data from a circular buffer.
 *
 * On entry, the sizep argument points to the size of the output buffer.
 * On exit, it is updated to the number of bytes actually transferred.
 *
 * If the buffer is empty, EAGAIN is returned, and the size of the
 * output buffer is undefined.
 */
int cbuf_pop(struct cbuf *cbuf, void *buf, size_t *sizep);

/*
 * Push a byte to a circular buffer.
 *
 * If the function isn't allowed to erase old data and the circular buffer
 * is full, EAGAIN is returned.
 */
int cbuf_pushb(struct cbuf *cbuf, uint8_t byte, bool erase);

/*
 * Pop a byte from a circular buffer.
 *
 * If the buffer is empty, EAGAIN is returned.
 */
int cbuf_popb(struct cbuf *cbuf, void *bytep);

/*
 * Write into a circular buffer at a specific location.
 *
 * If the given index is outside buffer boundaries, EINVAL is returned.
 * The given [index, size) range may extend beyond the end of the circular
 * buffer.
 */
int cbuf_write(struct cbuf *cbuf, size_t index, const void *buf, size_t size);

/*
 * Read from a circular buffer at a specific location.
 *
 * On entry, the sizep argument points to the size of the output buffer.
 * On exit, it is updated to the number of bytes actually transferred.
 *
 * If the given index is outside buffer boundaries, EINVAL is returned.
 *
 * The circular buffer isn't changed by this operation.
 */
int cbuf_read(const struct cbuf *cbuf, size_t index, void *buf, size_t *sizep);

#endif /* _CBUF_H */
