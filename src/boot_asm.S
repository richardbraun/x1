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

#include "asm.h"
#include "boot.h"
#include "cpu.h"

/*
 * Put the following instructions into the .text section, which is
 * allocatable and executable.
 */
.section .text, "ax"

/*
 * This symbol is the entry point, i.e. the first instruction that should
 * be run when control is passed to the kernel. The address of this symbol
 * is what the following command returns :
 *   readelf -aW x1 | grep "Entry point"
 */
ASM_FUNC(boot_start)
  mov %r0, %r13                     /* load boot stack in PSP */
  msr psp, %r0
  mov %r0, $2                       /* use PSP as stack pointer */
  msr control, %r0
  isb                               /* force CONTROL register evaluation,
                                       i.e. R13 is now PSP */
  ldr %r0, =cpu_exc_stack           /* load exception stack in MSP */
  add %r0, %r0, CPU_EXC_STACK_SIZE
  msr msp, %r0
  bl boot_clear_bss                 /* boot_clear_bss() */
  b boot_main                       /* goto boot_main */

/*
 * Clear the .bss section.
 *
 * The stack, being part of the .bss section, must not be used to save
 * data, since that data will be cleared.
 */
ASM_FUNC(boot_clear_bss)            /* void                             */
  mov %r0, #0                       /* boot_clear_bss(void)             */
  ldr %r1, =_bss_start              /* {                                */
  ldr %r2, =_bss_end                /*     uint32_t *r1 = &_bss_start;  */
                                    /*     uint32_t *r2 = &_bss_end;    */

1:
  cmp %r1, %r2                      /*     while (r1 < r2) {            */
  it hs                             /*         *r1 = 0;                 */
  bxhs %r14                         /*         r1++;                    */
  str %r0, [%r1]                    /*     }                            */
  add %r1, %r1, #4                  /* }                                */
  b 1b
