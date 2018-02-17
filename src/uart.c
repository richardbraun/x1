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
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/cbuf.h>
#include <lib/macros.h>

#include "cpu.h"
#include "io.h"
#include "uart.h"
#include "thread.h"

#define UART_BAUD_RATE          115200

#define UART_CLOCK              115200
#define UART_DIVISOR            (UART_CLOCK / UART_BAUD_RATE)

#define UART_IRQ                4

#define UART_IER_DATA           0x1

#define UART_LCR_8BITS          0x3
#define UART_LCR_STOP1          0
#define UART_LCR_PARITY_NONE    0
#define UART_LCR_DLAB           0x80

#define UART_LSR_DATA_READY     0x01
#define UART_LSR_TX_EMPTY       0x20

#define UART_COM1_PORT          0x3F8
#define UART_REG_DAT            0
#define UART_REG_DIVL           0
#define UART_REG_IER            1
#define UART_REG_DIVH           1
#define UART_REG_LCR            3
#define UART_REG_LSR            5

#define UART_BUFFER_SIZE        16

#if !ISP2(UART_BUFFER_SIZE)
#error "invalid buffer size"
#endif

/*
 * Data shared between threads and the interrupt handler.
 *
 * Interrupts and preemption must be disabled when accessing these data.
 */
static uint8_t uart_buffer[UART_BUFFER_SIZE];
static struct cbuf uart_cbuf;
static struct thread *uart_waiter;

static void
uart_irq_handler(void *arg)
{
    uint8_t byte;
    int error;
    bool spurious;

    (void)arg;

    spurious = true;

    for (;;) {
        byte = io_read(UART_COM1_PORT + UART_REG_LSR);

        if (!(byte & UART_LSR_DATA_READY)) {
            break;
        }

        spurious = false;
        byte = io_read(UART_COM1_PORT + UART_REG_DAT);
        error = cbuf_pushb(&uart_cbuf, byte, false);

        if (error) {
            printf("uart: error: buffer full\n");
            break;
        }
    }

    if (!spurious) {
        thread_wakeup(uart_waiter);
    }
}

void
uart_setup(void)
{
    cbuf_init(&uart_cbuf, uart_buffer, sizeof(uart_buffer));

    io_write(UART_COM1_PORT + UART_REG_LCR, UART_LCR_DLAB);
    io_write(UART_COM1_PORT + UART_REG_DIVL, UART_DIVISOR);
    io_write(UART_COM1_PORT + UART_REG_DIVH, UART_DIVISOR >> 8);
    io_write(UART_COM1_PORT + UART_REG_LCR, UART_LCR_8BITS | UART_LCR_STOP1
                                            | UART_LCR_PARITY_NONE);
    io_write(UART_COM1_PORT + UART_REG_IER, UART_IER_DATA);

    cpu_irq_register(UART_IRQ, uart_irq_handler, NULL);
}

static void
uart_tx_wait(void)
{
    uint8_t byte;

    for (;;) {
        byte = io_read(UART_COM1_PORT + UART_REG_LSR);

        if (byte & UART_LSR_TX_EMPTY) {
            break;
        }
    }
}

static void
uart_write_byte(uint8_t byte)
{
    uart_tx_wait();
    io_write(UART_COM1_PORT + UART_REG_DAT, byte);
}

void
uart_write(uint8_t byte)
{
    if (byte == '\n') {
        uart_write_byte('\r');
    }

    uart_write_byte(byte);
}

int
uart_read(uint8_t *byte)
{
    uint32_t eflags;
    int error;

    thread_preempt_disable();
    eflags = cpu_intr_save();

    if (uart_waiter) {
        error = EBUSY;
        goto out;
    }

    for (;;) {
        error = cbuf_popb(&uart_cbuf, byte);

        if (!error) {
            break;
        }

        uart_waiter = thread_self();
        thread_sleep();
        uart_waiter = NULL;
    }

    error = 0;

out:
    cpu_intr_restore(eflags);
    thread_preempt_enable();

    return error;
}
