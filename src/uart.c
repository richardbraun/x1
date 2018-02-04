/*
 * Copyright (c) 2017-2018 Richard Braun.
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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/cbuf.h>
#include <lib/macros.h>

#include "cpu.h"
#include "rcc.h"
#include "uart.h"
#include "thread.h"

#define UART_BAUD_RATE          115200
#define UART_CLK                RCC_FREQ_APB2

#define UART_USART6_ADDR        0x40011400
#define UART_USART6_IRQ         71

#define UART_SR_RXNE            0x00000020
#define UART_SR_TXE             0x00000080

#define UART_CR1_RE             0x00000004
#define UART_CR1_TE             0x00000008
#define UART_CR1_RXNEIE         0x00000020
#define UART_CR1_UE             0x00002000

#define UART_BRR_FRACTION_MASK  0x0000000f
#define UART_BRR_MANTISSA_MASK  0x0000fff0
#define UART_BRR_MANTISSA_SHIFT 4

#define UART_BUFFER_SIZE        16

#if !ISP2(UART_BUFFER_SIZE)
#error "invalid buffer size"
#endif

struct uart_regs {
    uint32_t sr;
    uint32_t dr;
    uint32_t brr;
    uint32_t cr1;
    uint32_t cr2;
    uint32_t cr3;
    uint32_t gtpr;
};

static volatile struct uart_regs *uart_regs = (void *)UART_USART6_ADDR;

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
    uint32_t reg;
    bool spurious;
    int error;

    (void)arg;

    spurious = true;

    for (;;) {
        reg = uart_regs->sr;

        if (!(reg & UART_SR_RXNE)) {
            break;
        }

        spurious = false;
        reg = uart_regs->dr;
        error = cbuf_pushb(&uart_cbuf, (uint8_t)reg, false);

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
    uint32_t divx100, mantissa, fraction;

    cbuf_init(&uart_cbuf, uart_buffer, sizeof(uart_buffer));

    divx100 = UART_CLK / (16 * (UART_BAUD_RATE / 100));
    fraction = ((divx100 % 100) * 16) / 100;
    mantissa = divx100 / 100;

    uart_regs->brr = ((mantissa << UART_BRR_MANTISSA_SHIFT)
                      & UART_BRR_MANTISSA_MASK)
                     | (fraction & UART_BRR_FRACTION_MASK);
    uart_regs->cr1 |= UART_CR1_UE
                      | UART_CR1_RXNEIE
                      | UART_CR1_TE
                      | UART_CR1_RE;

    cpu_irq_register(UART_USART6_IRQ, uart_irq_handler, NULL);
}

static void
uart_tx_wait(void)
{
    uint32_t sr;

    for (;;) {
        sr = uart_regs->sr;

        if (sr & UART_SR_TXE) {
            break;
        }
    }
}

static void
uart_write_byte(uint8_t byte)
{
    uart_tx_wait();
    uart_regs->dr = byte;
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
    int primask, error;

    primask = thread_preempt_disable_intr_save();

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
    thread_preempt_enable_intr_restore(primask);

    return error;
}
