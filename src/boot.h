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

#ifndef _BOOT_H
#define _BOOT_H

#include <lib/macros.h>

#include "cpu.h"

/*
 * The size of the boot stack.
 *
 * See the boot_stack variable in boot.c.
 */
#define BOOT_STACK_SIZE 512

#if !P2ALIGNED(BOOT_STACK_SIZE, CPU_STACK_ALIGN)
#error "misaligned boot stack"
#endif

#ifndef __ASSEMBLER__

#include <stdint.h>

extern uint8_t boot_stack[BOOT_STACK_SIZE];

/*
 * Entry point.
 */
void boot_start(void);

#endif /* __ASSEMBLER__ */

#endif /* _BOOT_H */
