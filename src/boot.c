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

#include <stdint.h>
#include <string.h>

#include <lib/macros.h>

#include "boot.h"
#include "cpu.h"
#include "main.h"

extern char _lma_data_addr;
extern char _data_start;
extern char _data_end;
extern char _bss_start;
extern char _bss_end;

void boot_main(void);

uint8_t boot_stack[BOOT_STACK_SIZE] __aligned(CPU_STACK_ALIGN);

static void
boot_copy_data(void)
{
    memcpy(&_data_start, &_lma_data_addr, &_data_end - &_data_start);
}

void
boot_main(void)
{
    cpu_intr_disable();
    boot_copy_data();
    main();
}
