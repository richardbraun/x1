/*
 * Copyright (c) 2017-2018 Richard Braun.
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
 * CPU services.
 *
 * The main functionality of this module is to provide interrupt control,
 * and registration of IRQ handlers.
 */

#ifndef _CPU_H
#define _CPU_H

#include <lib/macros.h>

#define CPU_FREQ 168000000

#define CPU_STACK_ALIGN 8

#define CPU_EXC_STACK_SIZE 4096

#if !P2ALIGNED(CPU_EXC_STACK_SIZE, CPU_STACK_ALIGN)
#error "misaligned exception stack"
#endif

#define CPU_EXC_RESET           1
#define CPU_EXC_NMI             2
#define CPU_EXC_HARDFAULT       3
#define CPU_EXC_MEMMANAGE       4
#define CPU_EXC_BUSFAULT        5
#define CPU_EXC_USAGEFAULT      6
#define CPU_EXC_SVCALL          11
#define CPU_EXC_DEBUGMONITOR    12
#define CPU_EXC_PENDSV          14
#define CPU_EXC_SYSTICK         15
#define CPU_EXC_IRQ_BASE        16
#define CPU_EXC_IRQ_MAX         255
#define CPU_NR_EXCEPTIONS       (CPU_EXC_IRQ_MAX + 1)
#define CPU_NR_IRQS             (CPU_NR_EXCEPTIONS - CPU_EXC_IRQ_BASE)

/*
 * PRIMASK register bits.
 */
#define CPU_PRIMASK_I           0x1

/*
 * Memory mapped processor registers.
 */
#define CPU_REG_ICSR            0xe000ed04

#define CPU_ICSR_PENDSVSET      0x10000000

#ifndef __ASSEMBLER__

#include <stdbool.h>
#include <stdint.h>

#include "thread.h"

/*
 * Type for IRQ handler functions.
 *
 * When called, interrupts and preemption are disabled.
 */
typedef void (*cpu_irq_handler_fn_t)(void *arg);

static inline void
cpu_inst_barrier(void)
{
    asm volatile("isb" : : : "memory");
}

static inline uint32_t
cpu_read_primask(void)
{
    uint32_t primask;

    asm volatile("mrs %0, primask" : "=r" (primask));
    return primask;
}

/*
 * Enable/disable interrupts.
 *
 * These functions imply a compiler barrier.
 * See thread_preempt_disable() in thread.c.
 */
static inline void
cpu_intr_disable(void)
{
    /*
     * The cpsid instruction is self-synchronizing and doesn't require
     * an instruction barrier.
     */
    asm volatile("cpsid i" : : : "memory");
}

static inline void
cpu_intr_enable(void)
{
    /*
     * The cpsie instruction isn't self-synchronizing. If pending interrupts
     * must be processed immediately, add an instruction barrier after.
     */
    asm volatile("cpsie i" : : : "memory");
}

/*
 * Disable/enable interrupts.
 *
 * Calls to these functions can safely nest.
 *
 * These functions imply a compiler barrier.
 * See thread_preempt_disable() in thread.c.
 */
static inline uint32_t
cpu_intr_save(void)
{
    uint32_t primask;

    primask = cpu_read_primask();
    cpu_intr_disable();
    return primask;
}

static inline void
cpu_intr_restore(uint32_t primask)
{
    asm volatile("msr primask, %0" : : "r" (primask) : "memory");
}

/*
 * Return true if interrupts are enabled.
 *
 * Implies a compiler barrier.
 */
static inline bool
cpu_intr_enabled(void)
{
    uint32_t primask;

    primask = cpu_read_primask();
    return !(primask & CPU_PRIMASK_I);
}

/*
 * Enter an idle state until the next interrupt.
 */
static inline void
cpu_idle(void)
{
#if LOW_POWER
    asm volatile("wfi" : : : "memory");
#endif
}

static inline void
cpu_raise_svcall(void)
{
    asm volatile("svc $0" : : : "memory");
}

static inline void
cpu_raise_pendsv(void)
{
    volatile uint32_t *icsr;

    icsr = (void *)CPU_REG_ICSR;
    *icsr = CPU_ICSR_PENDSVSET;
    cpu_inst_barrier();
}

/*
 * Completely halt execution on the processor.
 *
 * This function disables interrupts and enters an infinite idle loop.
 */
void cpu_halt(void) __attribute__((noreturn));

/*
 * Register an IRQ handler.
 *
 * When the given IRQ is raised, the handler function is called with the
 * given argument.
 */
void cpu_irq_register(unsigned int irq, cpu_irq_handler_fn_t fn, void *arg);

void * cpu_stack_forge(void *stack, size_t size, thread_fn_t fn, void *arg);

/*
 * Initialize the cpu module.
 */
void cpu_setup(void);

#endif /* __ASSEMBLER__ */

#endif /* _CPU_H */
