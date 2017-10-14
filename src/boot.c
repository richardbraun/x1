/*
 * Copyright (c) 2017 Richard Braun.
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

#include <stdint.h>

#include <lib/macros.h>

#include "boot.h"

/*
 * This is the boot stack, used by the boot code to set the value of
 * the ESP register very early once control is passed to the kernel.
 *
 * It is aligned to 4 bytes to comply with the System V Intel 386 ABI [1].
 * While not strictly required since x86 supports unaligned accesses,
 * aligned accesses are faster, and the compiler generates instructions
 * accessing the stack that assume it's aligned.
 *
 * See the assembly code at the boot_start label in boot_asm.S.
 *
 * [1] http://www.sco.com/developers/devspecs/abi386-4.pdf
 */
uint8_t boot_stack[BOOT_STACK_SIZE] __aligned(4);
