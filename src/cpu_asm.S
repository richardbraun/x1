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

#include "cpu.h"

.section .text
.code32

.global cpu_get_eflags
cpu_get_eflags:
  pushf
  pop %eax
  ret

.global cpu_set_eflags
cpu_set_eflags:
  mov 4(%esp), %eax
  push %eax
  popf
  ret

.global cpu_intr_enable
cpu_intr_enable:
  sti
  ret

.global cpu_intr_disable
cpu_intr_disable:
  cli
  ret

.global cpu_idle
cpu_idle:
  hlt
  ret

.global cpu_load_gdt
cpu_load_gdt:
  mov 4(%esp), %eax             /* eax = &desc */
  lgdt (%eax)                   /* lgdt(*eax) */

  mov $CPU_GDT_SEL_DATA, %eax
  mov %eax, %ds
  mov %eax, %es
  mov %eax, %fs
  mov %eax, %gs
  mov %eax, %ss

  /*
   * The code segment register cannot directly be written to, and is instead
   * modified by performing a long jump.
   *
   * See Intel 64 and IA-32 Architecture Software Developer's Manual :
   *  - Volume 2 Instruction Set Reference
   *    - 3.2 Instructions (A-L)
   *      - JMP
   *        - Far Jumps in Protected Mode
   *  - Volume 3 System Programming Guide:
   *    - 3.4.3 Segment Registers
   */
  ljmp $CPU_GDT_SEL_CODE, $1f

1:
  ret

.global cpu_load_idt
cpu_load_idt:
  mov 4(%esp), %eax             /* eax = &desc */
  lidt (%eax)                   /* lidt(*eax) */
  ret

/*
 * See struct cpu_intr_frame in cpu.c.
 */
.macro CPU_INTR_STORE_REGISTERS
  push %edi
  push %esi
  push %ebp
  push %edx
  push %ecx
  push %ebx
  push %eax
.endm

.macro CPU_INTR_LOAD_REGISTERS
  pop %eax
  pop %ebx
  pop %ecx
  pop %edx
  pop %ebp
  pop %esi
  pop %edi
.endm

/*
 * Some interrupts push an error code, and some don't.
 * Have a single interrupt frame layout by pushing a dummy error code.
 */
#define CPU_INTR(vector, name)          \
.global name;                           \
name:                                   \
  pushl $0;                             \
  pushl $(vector);                      \
  jmp cpu_intr_common

#define CPU_INTR_ERROR(vector, name)    \
.global name;                           \
name:                                   \
  pushl $(vector);                      \
  jmp cpu_intr_common

/*
 * This is the first common low level entry point for all exceptions and
 * interrupts. When reached, the stack contains the registers automatically
 * pushed by the processor, an error code and the vector. It's important
 * to note that the stack pointer still points to the stack of the thread
 * running when the interrupt occurs. Actually, in this implementation,
 * the entire interrupt handler borrows the stack of the interrupted thread.
 *
 * This is dangerous because the stack must then be large enough for both
 * the largest call chain of the interrupted thread as well as the largest
 * call chain of any interrupt handler. On some implementations, especially
 * those with very demanding real-time constraints, interrupt handling may
 * nest to avoid waiting for the current interrupt to be serviced before
 * starting handling a higher priority one, leading to even larger stack
 * needs. This is why many operating systems dedicate a separate stack for
 * interrupt handling.
 */
cpu_intr_common:
  CPU_INTR_STORE_REGISTERS
  push %esp                 /* push the address of the interrupt frame */
  call cpu_intr_main        /* cpu_intr_main(frame) */
  add $4, %esp              /* restore the stack pointer */
  CPU_INTR_LOAD_REGISTERS
  add $8, %esp              /* skip vector and error */
  iret                      /* return from interrupt */

CPU_INTR(CPU_IDT_VECT_DIV, cpu_isr_divide_error)
CPU_INTR_ERROR(CPU_IDT_VECT_GP, cpu_isr_general_protection)

/*
 * XXX There must be as many low level ISRs as there are possible IRQ vectors.
 *
 * See the i8259 module.
 */
CPU_INTR(32, cpu_isr_32)
CPU_INTR(33, cpu_isr_33)
CPU_INTR(34, cpu_isr_34)
CPU_INTR(35, cpu_isr_35)
CPU_INTR(36, cpu_isr_36)
CPU_INTR(37, cpu_isr_37)
CPU_INTR(38, cpu_isr_38)
CPU_INTR(39, cpu_isr_39)
CPU_INTR(40, cpu_isr_40)
CPU_INTR(41, cpu_isr_41)
CPU_INTR(42, cpu_isr_42)
CPU_INTR(43, cpu_isr_43)
CPU_INTR(44, cpu_isr_44)
CPU_INTR(45, cpu_isr_45)
CPU_INTR(46, cpu_isr_46)
CPU_INTR(47, cpu_isr_47)
