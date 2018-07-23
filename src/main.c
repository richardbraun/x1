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

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/macros.h>
#include <lib/shell.h>

#include "cpu.h"
#include "i8254.h"
#include "i8259.h"
#include "main.h"
#include "mem.h"
#include "panic.h"
#include "sw.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"

#define MAIN_SHELL_STACK_SIZE 4096

/*
 * XXX The Clang compiler apparently doesn't like the lack of prototype for
 * the main function in free standing mode.
 */
void main(void);

static struct shell_cmd_set main_shell_cmd_set;
static struct shell main_shell;

static int
main_getc(void *io_object)
{
    uint8_t byte;
    int error;

    (void)io_object;

    error = uart_read(&byte);

    if (error) {
        return EOF;
    }

    return byte;
}

static void
main_vfprintf(void *io_object, const char *format, va_list ap)
{
    (void)io_object;
    vprintf(format, ap);
}

static void
main_shell_run(void *arg)
{
    shell_run(arg);
}

static void
main_setup_shell(void)
{
    int error;

    shell_cmd_set_init(&main_shell_cmd_set);
    shell_init(&main_shell, &main_shell_cmd_set,
               main_getc, main_vfprintf, NULL);
    error = thread_create(NULL, main_shell_run, &main_shell, "shell",
                          MAIN_SHELL_STACK_SIZE, THREAD_MIN_PRIORITY);

    if (error) {
        panic("main: unable to create shell thread");
    }
}

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
    i8259_setup();
    i8254_setup();
    uart_setup();
    mem_setup();
    thread_setup();
    timer_setup();
    main_setup_shell();
    sw_setup();

    printf("X1 " QUOTE(VERSION) "\n\n");

    thread_enable_scheduler();

    /* Never reached */
}

struct shell_cmd_set *
main_get_shell_cmd_set(void)
{
    return &main_shell_cmd_set;
}
