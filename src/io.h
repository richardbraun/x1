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
 * I/O ports access.
 *
 * The x86 architecture is special in that, in addition to the physical memory
 * address space, it also has an I/O port space. Most modern processors use
 * the physical memory address space to access memory-mapped device memory and
 * registers, and that's also the case on x86, but the I/O port space is also
 * used for this purpose, at least for some legacy devices.
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

/*
 * Read a byte from an I/O port.
 */
uint8_t io_read(uint16_t port);

/*
 * Write a byte to an I/O port.
 */
void io_write(uint16_t port, uint8_t byte);

#endif /* IO_H */
