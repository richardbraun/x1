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
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/list.h>
#include <lib/macros.h>

#include "cpu.h"
#include "mutex.h"
#include "panic.h"
#include "thread.h"
#include "timer.h"

#define TIMER_STACK_SIZE 4096

/*
 * When determining whether a point in time is in the future or the past,
 * it's important to remember that the value is always finite. Here,
 * the ticks use a 32-bits type (unsigned long on i386), so, assuming
 * a 100Hz frequency, time wraps around about every 497 days. Therefore,
 * the implementation needs a way to partition time between the future
 * and the past. To do so, it considers that all values from a reference
 * up to a threshold are in the future (except the present), and all other
 * values are in the past. This macro defines that threshold.
 *
 * Currently, the threshold is half of the maximum value, which is
 * equivalent to using a signed integer where positive values would denote
 * the future and negative ones the past, except that overflows on signed
 * integers are undefined behaviour, whereas overflows on unsigned
 * integers are well specified (3.4.3 Undefined Behaviour and 6.2.5 Types).
 */
#define TIMER_THRESHOLD (((unsigned long)-1) / 2)

/*
 * The current time, in ticks.
 */
static unsigned long timer_ticks;

/*
 * List of timers, sorted by time.
 *
 * The timer mutex must be locked when accessing the timer list.
 */
static struct list timer_list;
static struct mutex timer_mutex;

/*
 * The timer thread, which provides context for all timer callbacks.
 */
static struct thread *timer_thread;

/*
 * True if the timer list is empty.
 *
 * This is a copy of list_empty(&timer_list) which may be used from
 * interrupt context, where locking a mutex is impossible.
 *
 * Interrupts must be disabled when accessing this variable.
 */
static bool timer_list_empty;

/*
 * Time in ticks of the first timer on the timer list.
 *
 * Only valid if the list isn't empty.
 *
 * Interrupts must be disabled when accessing this variable.
 */
static unsigned long timer_wakeup_ticks;

bool
timer_ticks_expired(unsigned long ticks, unsigned long ref)
{
    return (ticks - ref) > TIMER_THRESHOLD;
}

bool
timer_ticks_occurred(unsigned long ticks, unsigned long ref)
{
    return (ticks == ref) || timer_ticks_expired(ticks, ref);
}

static bool
timer_work_pending(void)
{
    assert(!cpu_intr_enabled());

    return !timer_list_empty
           && timer_ticks_occurred(timer_wakeup_ticks, timer_ticks);
}

static bool
timer_scheduled(const struct timer *timer)
{
    return !list_node_unlinked(&timer->node);
}

static bool
timer_expired(const struct timer *timer, unsigned long ref)
{
    return timer_ticks_expired(timer->ticks, ref);
}

static bool
timer_occurred(const struct timer *timer, unsigned long ref)
{
    return timer_ticks_occurred(timer->ticks, ref);
}

static void
timer_process(struct timer *timer)
{
    timer->fn(timer->arg);
}

static void
timer_process_list(unsigned long now)
{
    struct timer *timer;
    uint32_t primask;

    mutex_lock(&timer_mutex);

    while (!list_empty(&timer_list)) {
        timer = list_first_entry(&timer_list, typeof(*timer), node);

        if (!timer_occurred(timer, now)) {
            break;
        }

        list_remove(&timer->node);
        list_node_init(&timer->node);
        mutex_unlock(&timer_mutex);

        timer_process(timer);

        mutex_lock(&timer_mutex);
    }

    primask = cpu_intr_save();

    timer_list_empty = list_empty(&timer_list);

    if (!timer_list_empty) {
        timer = list_first_entry(&timer_list, typeof(*timer), node);
        timer_wakeup_ticks = timer->ticks;
    }

    cpu_intr_restore(primask);

    mutex_unlock(&timer_mutex);
}

static void
timer_run(void *arg)
{
    unsigned long now;
    uint32_t primask;

    (void)arg;

    for (;;) {
        primask = thread_preempt_disable_intr_save();

        for (;;) {
            now = timer_ticks;

            if (timer_work_pending()) {
                break;
            }

            thread_sleep();
        }

        thread_preempt_enable_intr_restore(primask);

        timer_process_list(now);
    }
}

void
timer_setup(void)
{
    int error;

    timer_ticks = 0;
    timer_list_empty = true;

    list_init(&timer_list);
    mutex_init(&timer_mutex);

    error = thread_create(&timer_thread, timer_run, NULL,
                          "timer", TIMER_STACK_SIZE, THREAD_MIN_PRIORITY);
                          //"timer", TIMER_STACK_SIZE, THREAD_MAX_PRIORITY);

    if (error) {
        panic("timer: unable to create thread");
    }
}

unsigned long
timer_now(void)
{
    unsigned long ticks;
    uint32_t primask;

    primask = cpu_intr_save();
    ticks = timer_ticks;
    cpu_intr_restore(primask);

    return ticks;
}

void
timer_init(struct timer *timer, timer_fn_t fn, void *arg)
{
    list_node_init(&timer->node);
    timer->fn = fn;
    timer->arg = arg;
}

unsigned long
timer_get_time(const struct timer *timer)
{
    unsigned long ticks;

    mutex_lock(&timer_mutex);
    ticks = timer->ticks;
    mutex_unlock(&timer_mutex);

    return ticks;
}

void
timer_schedule(struct timer *timer, unsigned long ticks)
{
    struct timer *tmp;
    uint32_t primask;

    mutex_lock(&timer_mutex);

    assert(!timer_scheduled(timer));

    timer->ticks = ticks;

    /*
     * Find the insertion point,
     *
     * This makes timer scheduling an O(n) operation, and assumes a low
     * number of timers. This is also why a mutex is used instead of
     * disabling preemption, since preemption then remains enabled,
     * allowing higher priority threads to run.
     *
     * [1] https://en.wikipedia.org/wiki/Big_O_notation
     */
    list_for_each_entry(&timer_list, tmp, node) {
        if (!timer_expired(tmp, ticks)) {
            break;
        }
    }

    list_insert_before(&timer->node, &tmp->node);

    timer = list_first_entry(&timer_list, typeof(*timer), node);

    /*
     * This interrupt-based critical section could be moved outside the
     * mutex-based one, which would make the latter shorter. The downside
     * of this approach is that a timer interrupt occurring after unlocking
     * the mutex but before disabling interrupts will wake up the timer
     * thread, if there was work pending already. The timer thread may
     * preempt the current thread and process all timers before the current
     * thread resumes and clears the list empty flag. At the next timer
     * interrupt, the handler will determine that there is work pending and
     * wake up the timer thread, despite the fact that there is actually no
     * timer in the list.
     *
     * By holding the mutex while clearing the list empty flag, potential
     * spurious wake-ups are completely avoided.
     */
    primask = cpu_intr_save();
    timer_list_empty = false;
    timer_wakeup_ticks = timer->ticks;
    cpu_intr_restore(primask);

    mutex_unlock(&timer_mutex);
}

void
timer_report_tick(void)
{
    timer_ticks++;

    if (timer_work_pending()) {
        thread_wakeup(timer_thread);
    }
}
