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

#include "boot.h"

/*
 * These are values used in the OS image header, as defined by the multiboot
 * specification.
 *
 * See https://www.gnu.org/software/grub/manual/multiboot/multiboot.html.
 */
#define BOOT_HDR_MAGIC  0x1BADB002
#define BOOT_HDR_CHECK  0x2BADB002
#define BOOT_HDR_FLAGS  0x0

/*
 * The .section directive tells the assembler which section the following
 * instructions should go into.
 *
 * The "a" flag makes the section allocatable, meaning memory will be
 * allocated for that section at load time.
 *
 * See https://sourceware.org/binutils/docs-2.29/as/Section.html#Section.
 */
.section .hdr, "a"

/* Generate code for i386 */
.code32

/*
 * The .int directive is used to emit verbatim machine words. Here, the
 * third word is the checksum of the first two, defined as "a 32-bit
 * unsigned value which, when added to the other magic fields (i.e.
 * ‘magic’ and ‘flags’), must have a 32-bit unsigned sum of zero".
 * Intuitively, adding the two first words and making the result negative
 * gives a value that, when added to the other fields, gives 0, despite
 * the word being unsigned. This trick works because values use two's
 * complement representation.
 *
 * See https://en.wikipedia.org/wiki/Two%27s_complement.
 */
.int BOOT_HDR_MAGIC
.int BOOT_HDR_FLAGS
.int -(BOOT_HDR_FLAGS + BOOT_HDR_MAGIC)

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
 *
 * The .global directive tells the assembler to make the symbol global,
 * i.e. to make it visible to other compilation units.
 *
 * When this code is run, the machine state should comply with what the
 * multiboot specification defines.
 */
.global boot_start
boot_start:
  cmp $BOOT_HDR_CHECK, %eax     /* Compare EAX against the expected value */
  jne .                         /* If not equal, jump to the current address.
                                   This is an infinite loop. */
  mov $boot_stack, %esp         /* Set up a stack */
  add $BOOT_STACK_SIZE, %esp    /* On x86, stacks grow downwards, so start
                                   at the top */
  jmp main                      /* Jump to the C main function */

loop:
  hlt                           /* Never reached, for safety */
  jmp loop
