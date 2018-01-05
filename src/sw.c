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
 *
 *
 * Stopwatch demo application.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <lib/macros.h>
#include <lib/shell.h>

#include "condvar.h"
#include "mutex.h"
#include "panic.h"
#include "sw.h"
#include "thread.h"
#include "timer.h"

/*
 * Display interval, in seconds.
 */
#define SW_DISPLAY_INTERVAL 5

/*
 * Maximum wait time for the sw_wait command.
 */
#define SW_MAX_WAIT 30

/*
 * Stopwatch type.
 *
 * The mutex must be locked before accessing any member.
 */
struct sw {
    struct mutex mutex;
    struct condvar cv;
    struct timer timer;
    unsigned long ticks;
    bool timer_scheduled;
    bool thread_waiting;
    unsigned long wait_ticks;
};

/*
 * Singleton instance.
 */
static struct sw *sw_instance;

static struct sw *
sw_get_instance(void)
{
    assert(sw_instance);
    return sw_instance;
}

static void
sw_timer_run(void *arg)
{
    struct sw *sw;

    sw = arg;

    mutex_lock(&sw->mutex);

    if (!sw->timer_scheduled) {
        goto out;
    }

    sw->ticks++;

    if ((sw->ticks % (THREAD_SCHED_FREQ * SW_DISPLAY_INTERVAL)) == 0) {
        printf("%lu\n", sw->ticks);
    }

    if (sw->thread_waiting && timer_ticks_occurred(sw->wait_ticks, sw->ticks)) {
        sw->thread_waiting = false;
        condvar_signal(&sw->cv);
    }

    timer_schedule(&sw->timer, timer_get_time(&sw->timer) + 1);

out:
    mutex_unlock(&sw->mutex);
}

static struct sw *
sw_create(void)
{
    struct sw *sw;

    sw = malloc(sizeof(*sw));

    if (!sw) {
        return NULL;
    }

    mutex_init(&sw->mutex);
    condvar_init(&sw->cv);
    timer_init(&sw->timer, sw_timer_run, sw);
    sw->timer_scheduled = false;
    sw->thread_waiting = false;
    return sw;
}

static void
sw_schedule(struct sw *sw)
{
    if (sw->timer_scheduled) {
        return;
    }

    sw->timer_scheduled = true;
    timer_schedule(&sw->timer, timer_now() + 1);
}

static void
sw_start(struct sw *sw)
{
    mutex_lock(&sw->mutex);
    sw->ticks = 0;
    sw_schedule(sw);
    mutex_unlock(&sw->mutex);
}

static void
sw_stop(struct sw *sw)
{
    mutex_lock(&sw->mutex);
    sw->timer_scheduled = false;
    mutex_unlock(&sw->mutex);
}

static void
sw_resume(struct sw *sw)
{
    mutex_lock(&sw->mutex);
    sw_schedule(sw);
    mutex_unlock(&sw->mutex);
}

static unsigned long
sw_read(struct sw *sw)
{
    unsigned long ticks;

    mutex_lock(&sw->mutex);
    ticks = sw->ticks;
    mutex_unlock(&sw->mutex);

    return ticks;
}

static void
sw_wait(struct sw *sw, unsigned long seconds)
{
    mutex_lock(&sw->mutex);

    if (!sw->timer_scheduled) {
        printf("sw_wait: error: stopwatch disabled\n");
        goto out;
    } else if (sw->thread_waiting) {
        printf("sw_wait: error: thread already waiting\n");
        goto out;
    }

    sw->thread_waiting = true;
    sw->wait_ticks = sw->ticks + (seconds * THREAD_SCHED_FREQ);

    do {
        condvar_wait(&sw->cv, &sw->mutex);
    } while (sw->thread_waiting);

out:
    mutex_unlock(&sw->mutex);
}

static void
sw_shell_start(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sw_start(sw_get_instance());
}

static void
sw_shell_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sw_stop(sw_get_instance());
}

static void
sw_shell_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sw_resume(sw_get_instance());
}

static void
sw_shell_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("%lu\n", sw_read(sw_get_instance()));
}

static void
sw_shell_wait(int argc, char **argv)
{
    unsigned long seconds;
    int ret;

    if (argc != 2) {
        goto error;
    }

    ret = sscanf(argv[1], "%lu", &seconds);

    if ((ret != 1) || (seconds > SW_MAX_WAIT)) {
        goto error;
    }

    sw_wait(sw_get_instance(), seconds);
    return;

error:
    printf("sw_wait: error: invalid arguments\n");
}

static struct shell_cmd sw_shell_cmds[] = {
    SHELL_CMD_INITIALIZER("sw_start", sw_shell_start,
        "sw_start",
        "start the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_stop", sw_shell_stop,
        "sw_stop",
        "stop the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_resume", sw_shell_resume,
        "sw_resume",
        "resume the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_read", sw_shell_read,
        "sw_read",
        "read the stopwatch time"),
    SHELL_CMD_INITIALIZER("sw_wait", sw_shell_wait,
        "sw_wait <seconds>",
        "wait for up to " QUOTE(SW_MAX_WAIT) " seconds"),
};

void
sw_setup(void)
{
    sw_instance = sw_create();

    if (!sw_instance) {
        panic("sw: error: unable to create stopwatch");
    }

    SHELL_REGISTER_CMDS(sw_shell_cmds);
}
