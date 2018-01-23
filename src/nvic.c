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
#include "nvic.h"

#define NVIC_BASE_ADDR 0xe000e100

struct nvic_regs {
    uint32_t iser[16];
    uint32_t icer[16];
    uint32_t ispr[16];
    uint32_t icpr[16];
    uint32_t iabr[16];
    uint32_t reserved[47];
    uint32_t ipr[124];
};

static volatile struct nvic_regs *nvic_regs = (void *)NVIC_BASE_ADDR;

static void
nvic_get_dest(unsigned int irq, volatile uint32_t *array,
              volatile uint32_t **reg, uint32_t *mask)
{
    *reg = &array[irq / 32];
    *mask = (1 << (irq % 32));
}

void
nvic_irq_enable(unsigned int irq)
{
    volatile uint32_t *reg;
    uint32_t mask;
    uint32_t primask;

    nvic_get_dest(irq, nvic_regs->iser, &reg, &mask);

    primask = cpu_intr_save();
    *reg |= mask;
    cpu_intr_restore(primask);
}

void
nvic_irq_disable(unsigned int irq)
{
    volatile uint32_t *reg;
    uint32_t mask;
    uint32_t primask;

    nvic_get_dest(irq, nvic_regs->icer, &reg, &mask);

    primask = cpu_intr_save();
    *reg |= mask;
    cpu_intr_restore(primask);
}
