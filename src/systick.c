/*
 * Copyright (c) 2018 Richard Braun.
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

#include <stdint.h>

#include "cpu.h"
#include "panic.h"
#include "systick.h"
#include "thread.h"

#define SYSTICK_BASE_ADDR 0xe000e010

#define SYSTICK_CSR_ENABLE      0x1
#define SYSTICK_CSR_TICKINT     0x2

#define SYSTICK_CALIB_NOREF         0x80000000
#define SYSTICK_CALIB_SKEW          0x40000000
#define SYSTICK_CALIB_TENMS_MASK    0x00ffffff

struct systick_regs {
    uint32_t csr;
    uint32_t rvr;
    uint32_t cvr;
    uint32_t calib;
};

static volatile struct systick_regs *systick_regs = (void *)SYSTICK_BASE_ADDR;

static void
systick_check_calib(void)
{
    uint32_t calib;

    calib = systick_regs->calib;

    if (calib & SYSTICK_CALIB_NOREF) {
        panic("systick: unusable");
    }
}

void
systick_setup(void)
{
    uint32_t counter;

    systick_check_calib();

    counter = (CPU_FREQ / 8) / THREAD_SCHED_FREQ;
    systick_regs->rvr = counter;
    systick_regs->cvr = 0;
    systick_regs->csr = (SYSTICK_CSR_TICKINT | SYSTICK_CSR_ENABLE);
}
