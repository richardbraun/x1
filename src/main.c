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

#include <stdio.h>

#include <lib/macros.h>
#include <lib/shell.h>

#include "cpu.h"
#include "led.h"
#include "main.h"
#include "mem.h"
#include "panic.h"
#include "sw.h"
#include "systick.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"

/*
 * This function is the main entry point for C code. It's called from
 * assembly code in the boot module, very soon after control is passed
 * to the kernel.
 */
void
main(void)
{
    thread_bootstrap();
    cpu_setup();
    uart_setup();
    systick_setup();
    mem_setup();
    thread_setup();
    timer_setup();
    shell_setup();
    led_setup();
    sw_setup();

    printf("X1 " QUOTE(VERSION) "\n\n");

    thread_enable_scheduler();

    /* Never reached */
}
