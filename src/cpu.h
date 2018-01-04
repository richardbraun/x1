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
 * CPU services.
 *
 * The main functionality of this module is to provide interrupt control,
 * and registration of IRQ handlers.
 *
 * See the i8259 module.
 */

#ifndef _CPU_H
#define _CPU_H

/*
 * EFLAGS register flags.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 3
 * System Programming Guide, 2.3 System Flags and Fields in The EFLAGS Register.
 */
#define CPU_EFL_ONE     0x002
#define CPU_EFL_IF      0x200

/*
 * GDT segment descriptor indexes, in bytes.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 3
 * System Programming Guide, 3.4.1 Segment Descriptor Tables.
 */
#define CPU_GDT_SEL_NULL    0x00
#define CPU_GDT_SEL_CODE    0x08
#define CPU_GDT_SEL_DATA    0x10
#define CPU_GDT_SIZE        3

/*
 * IDT segment descriptor indexes (exception and interrupt vectors).
 *
 * There are actually a lot more potential exceptions on x86. This list
 * only includes vectors that are handled by the implementation.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 3
 * System Programming Guide, 6.3 Sources of Interrupts.
 */
#define CPU_IDT_VECT_DIV            0   /* Divide error */
#define CPU_IDT_VECT_GP             13  /* General protection fault */
#define CPU_IDT_VECT_IRQ_BASE       32  /* Base vector for external IRQs */

/*
 * Preprocessor declarations may be included by assembly source files, but
 * C declarations may not.
 */
#ifndef __ASSEMBLER__

#include <stdbool.h>
#include <stdint.h>

/*
 * Type for IRQ handler functions.
 *
 * When called, interrupts and preemption are disabled.
 */
typedef void (*cpu_irq_handler_fn_t)(void *arg);

/*
 * Enable/disable interrupts.
 *
 * These functions imply a compiler barrier.
 * See thread_preempt_disable() in thread.c.
 */
void cpu_intr_enable(void);
void cpu_intr_disable(void);

/*
 * Disable/restore interrupts.
 *
 * Calls to these functions can safely nest.
 *
 * These functions imply a compiler barrier.
 * See thread_preempt_disable() in thread.c.
 */
uint32_t cpu_intr_save(void);
void cpu_intr_restore(uint32_t eflags);

/*
 * Return true if interrupts are enabled.
 *
 * Implies a compiler barrier.
 */
bool cpu_intr_enabled(void);

/*
 * Enter an idle state until the next interrupt.
 */
void cpu_idle(void);

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

/*
 * Initialize the cpu module.
 */
void cpu_setup(void);

#endif /* __ASSEMBLER__ */

#endif /* _CPU_H */
