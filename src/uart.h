/*
 * Copyright (c) 2017 Richard Braun.
 * Copyright (c) 2017 Jerko Lenstra.
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
 * UART driver.
 *
 * An UART is a Universal Asynchronous Receiver-Transmitter, an old, low level
 * interface used for machine communication. It's typically used on embedded
 * devices as the primary diagnostic interface, especially during development.
 * It may also be used between remote boards that don't require fast
 * communication.
 */

#ifndef _UART_H
#define _UART_H

#include <stdint.h>

/*
 * Initialize the uart module.
 */
void uart_setup(void);

/*
 * Write a byte to the UART.
 */
void uart_write(uint8_t byte);

/*
 * Read a byte from the UART.
 *
 * This function may only be called from thread context, since it blocks
 * until there is data to consume.
 *
 * If successful, return 0. If another thread is already waiting for data,
 * ERROR_BUSY is returned.
 *
 * Preemption must be enabled when calling this function.
 */
int uart_read(uint8_t *byte);

#endif /* _UART_H */
