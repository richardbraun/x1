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

#include "asm.h"
#include "boot.h"
#include "cpu.h"

.section .text, "ax"

.macro CPU_EXC_STORE_REGISTERS
  stmfd %r0!, {%r4-%r11}
.endm

.macro CPU_EXC_LOAD_REGISTERS
  ldmfd %r0!, {%r4-%r11}
.endm

ASM_FUNC(cpu_exc_svcall)
  stmfd %r13!, {%r14}           /* save R14 */
  bl thread_yield_from_svcall   /* thread_yield_from_svcall() */
  CPU_EXC_LOAD_REGISTERS
  msr psp, %r0                  /* reload stack in case of context switch */
  ldmfd %r13!, {%r14}           /* restore R14 */
  bx %r14                       /* return from exception */

ASM_FUNC(cpu_exc_pendsv)
  stmfd %r13!, {%r14}           /* save R14 */
  mrs %r0, psp                  /* pass the new stack pointer as argument 0 */
  CPU_EXC_STORE_REGISTERS
  bl thread_yield_from_pendsv   /* thread_yield_from_pendsv(sp) */
  CPU_EXC_LOAD_REGISTERS
  msr psp, %r0                  /* reload stack in case of context switch */
  ldmfd %r13!, {%r14}           /* restore R14 */
  bx %r14                       /* return from exception */
