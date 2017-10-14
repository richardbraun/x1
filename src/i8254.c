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

#include <lib/macros.h>

#include "cpu.h"
#include "i8254.h"
#include "io.h"
#include "thread.h"

#define I8254_FREQ                  1193182

#define I8254_PORT_CHANNEL0         0x40
#define I8254_PORT_MODE             0x43

#define I8254_CONTROL_BINARY        0x00
#define I8254_CONTROL_RATE_GEN      0x04
#define I8254_CONTROL_RW_LSB        0x10
#define I8254_CONTROL_RW_MSB        0x20
#define I8254_CONTROL_COUNTER0      0x00

#define I8254_INITIAL_COUNT         DIV_CEIL(I8254_FREQ, THREAD_SCHED_FREQ)

#define I8254_IRQ                   0

static void
i8254_irq_handler(void *arg)
{
    (void)arg;
    thread_report_tick();
}

void
i8254_setup(void)
{
    uint16_t value;

    /*
     * Program the timer to raise an interrupt at the scheduling frequency.
     */

    io_write(I8254_PORT_MODE, I8254_CONTROL_COUNTER0
                              | I8254_CONTROL_RW_MSB
                              | I8254_CONTROL_RW_LSB
                              | I8254_CONTROL_RATE_GEN
                              | I8254_CONTROL_BINARY);

    value = I8254_INITIAL_COUNT;
    io_write(I8254_PORT_CHANNEL0, value & 0xff);
    io_write(I8254_PORT_CHANNEL0, value >> 8);

    cpu_irq_register(I8254_IRQ, i8254_irq_handler, NULL);
}
