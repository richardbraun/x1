/*
 * Copyright (c) 2018 Richard Braun.
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

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <lib/shell.h>

#include "gpio.h"
#include "led.h"
#include "thread.h"
#include "timer.h"

#define LED_BLINK_INTERVAL THREAD_SCHED_FREQ

static struct timer led_timer;
static bool led_on;
static volatile bool led_blinking_enabled;

static void
led_shell_blink(int argc, char **argv)
{
    bool enabled;

    if (argc > 2) {
        goto error;
    } else if (argc == 1) {
        printf("led: blinking: %s\n", led_blinking_enabled ? "yes" : "no");
        return;
    }

    if (strcmp(argv[1], "on") == 0) {
        enabled = true;
    } else if (strcmp(argv[1], "off") == 0) {
        enabled = false;
    } else {
        goto error;
    }

    led_blinking_enabled = enabled;
    return;

error:
    printf("led: error: invalid arguments\n");
}

static struct shell_cmd led_shell_cmds[] = {
    SHELL_CMD_INITIALIZER("led_blink", led_shell_blink,
        "led_blink [on|off]",
        "control led blinking"),
};

static void
led_toggle(void *arg)
{
    (void)arg;

    if (led_blinking_enabled) {
        if (led_on) {
            gpio_led_off();
        } else {
            gpio_led_on();
        }
    }

    led_on = !led_on;
    timer_schedule(&led_timer, timer_get_time(&led_timer) + LED_BLINK_INTERVAL);
}

void
led_setup(void)
{
    gpio_led_off();
    led_on = false;
    led_blinking_enabled = true;

    SHELL_REGISTER_CMDS(led_shell_cmds);

    timer_init(&led_timer, led_toggle, NULL);
    timer_schedule(&led_timer, timer_now() + LED_BLINK_INTERVAL);
}
