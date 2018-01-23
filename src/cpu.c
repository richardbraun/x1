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
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/macros.h>

#include "boot.h"
#include "cpu.h"
#include "nvic.h"
#include "thread.h"
#include "timer.h"

/*
 * xPSR register bits.
 */
#define CPU_PSR_8BYTE_STACK_ALIGN   0x00000200
#define CPU_PSR_THUMB               0x01000000

/*
 * Declarations for C/assembly functions that are global so that they can
 * be shared between cpu.c and cpu_asm.S, but are considered private to
 * the cpu module.
 */
void cpu_exc_main(void);
void cpu_exc_svcall(void);
void cpu_exc_pendsv(void);
void cpu_irq_main(void);

/*
 * Exception vector table.
 */
static const void *cpu_vector_table[] __used __section(".vectors") = {
    [0]                                     = &boot_stack[ARRAY_SIZE(boot_stack)],
    [CPU_EXC_RESET]                         = boot_start,
    [CPU_EXC_NMI ... CPU_EXC_USAGEFAULT]    = cpu_exc_main,
    [CPU_EXC_SVCALL]                        = cpu_exc_svcall,
    [CPU_EXC_DEBUGMONITOR]                  = cpu_exc_main,
    [CPU_EXC_PENDSV]                        = cpu_exc_pendsv,
    [CPU_EXC_SYSTICK]                       = cpu_exc_main,
    [CPU_EXC_IRQ_BASE ... CPU_EXC_IRQ_MAX]  = cpu_irq_main,
};

/*
 * Handler for external interrupt requests.
 */
struct cpu_irq_handler {
    cpu_irq_handler_fn_t fn;
    void *arg;
};

/*
 * Array where driver IRQ handlers are registered.
 *
 * Interrupts and preemption must be disabled when accessing the handlers.
 */
static struct cpu_irq_handler cpu_irq_handlers[CPU_NR_IRQS];

/*
 * The exception frame is the stack content forged by exception handlers.
 * They store the data needed to restore the processor to its state prior
 * to the exception.
 */
struct cpu_exc_frame {
    /* These members are pushed by cpu_exc_pendsv() */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;

    /* These members are automatically pushed by the CPU */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t r14;
    uint32_t r15;
    uint32_t psr;
};

uint8_t cpu_exc_stack[CPU_EXC_STACK_SIZE] __aligned(CPU_STACK_ALIGN);

void
cpu_halt(void)
{
    cpu_intr_disable();

    for (;;) {
        cpu_idle();
    }
}

static void
cpu_irq_handler_init(struct cpu_irq_handler *handler)
{
    handler->fn = NULL;
}

static struct cpu_irq_handler *
cpu_get_irq_handler(unsigned int irq)
{
    assert(irq < ARRAY_SIZE(cpu_irq_handlers));
    return &cpu_irq_handlers[irq];
}

static void
cpu_irq_handler_set_fn(struct cpu_irq_handler *handler,
                       cpu_irq_handler_fn_t fn, void *arg)
{
    assert(handler->fn == NULL);
    handler->fn = fn;
    handler->arg = arg;
}

static inline uint32_t
cpu_read_ipsr(void)
{
    uint32_t vector;

    asm volatile("mrs %0, ipsr" : "=r" (vector));
    return vector;
}

void
cpu_exc_main(void)
{
    uint32_t vector, primask;

    vector = cpu_read_ipsr();

    assert(vector < CPU_EXC_IRQ_BASE);

    /*
     * Interrupt handlers may call functions that may in turn yield the
     * processor. When running in interrupt context, as opposed to thread
     * context, there is no way to yield the processor, because the context
     * isn't saved into a scheduled structure, which is what threads are
     * for. As a result, disable preemption to prevent an invalid context
     * switch.
     */
    primask = thread_preempt_disable_intr_save();

    switch (vector) {
    case CPU_EXC_SYSTICK:
        thread_report_tick();
        timer_report_tick();
        break;
    default:
        printf("cpu: error: unhandled exception:%lu\n", (unsigned long)vector);
        cpu_halt();
    }

    thread_preempt_enable_intr_restore(primask);
}

void
cpu_irq_main(void)
{
    struct cpu_irq_handler *handler;
    uint32_t primask;
    unsigned int irq;

    irq = cpu_read_ipsr() - CPU_EXC_IRQ_BASE;

    primask = thread_preempt_disable_intr_save();

    handler = cpu_get_irq_handler(irq);

    if (!handler || !handler->fn) {
        panic("cpu: error: invalid handler for irq %u", irq);
    }

    handler->fn(handler->arg);

    thread_preempt_enable_intr_restore(primask);
}

void
cpu_irq_register(unsigned int irq, cpu_irq_handler_fn_t fn, void *arg)
{
    struct cpu_irq_handler *handler;
    uint32_t primask;

    primask = thread_preempt_disable_intr_save();

    handler = cpu_get_irq_handler(irq);
    cpu_irq_handler_set_fn(handler, fn, arg);
    nvic_irq_enable(irq);

    thread_preempt_enable_intr_restore(primask);
}

void *
cpu_stack_forge(void *stack, size_t size, thread_fn_t fn, void *arg)
{
    struct cpu_exc_frame *frame;

    assert(P2ALIGNED((uintptr_t)stack, CPU_STACK_ALIGN));

    if (size <= sizeof(*frame)) {
        panic("cpu: error: stack too small");
    }

    frame = stack + size;
    frame--;

    frame->r4  = 4;
    frame->r5  = 5;
    frame->r6  = 6;
    frame->r7  = 7;
    frame->r8  = 8;
    frame->r9  = 9;
    frame->r10 = 10;
    frame->r11 = 11;
    frame->r0  = (uint32_t)fn;
    frame->r1  = (uint32_t)arg;
    frame->r2  = 2;
    frame->r3  = 3;
    frame->r12 = 12;
    frame->r14 = 0;
    frame->r15 = (uint32_t)thread_main & ~1; /* Must be halfword aligned */
    frame->psr = CPU_PSR_THUMB;

    return frame;
}

void
cpu_setup(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(cpu_irq_handlers); i++) {
        cpu_irq_handler_init(cpu_get_irq_handler(i));
    }
}
