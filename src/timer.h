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
 * Software timer module.
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <stdbool.h>

#include <lib/list.h>

/*
 * Type for timer callback functions.
 *
 * These functions run in the context of the timer thread.
 */
typedef void (*timer_fn_t)(void *arg);

struct timer {
    struct list node;
    unsigned long ticks;
    timer_fn_t fn;
    void *arg;
};

/*
 * Check if a time, in ticks, is considered to have expired/occurred
 * compared to a given reference time, also in ticks.
 *
 * A time is considered expired when it's strictly in the past compared
 * to the given reference time, and occurred when it's expired or equal
 * to the reference time.
 */
bool timer_ticks_expired(unsigned long ticks, unsigned long ref);
bool timer_ticks_occurred(unsigned long ticks, unsigned long ref);

/*
 * Initialize the timer module.
 */
void timer_setup(void);

/*
 * Return the current time, in ticks.
 */
unsigned long timer_now(void);

/*
 * Initialize a timer.
 *
 * A timer may only be safely initialized when not scheduled.
 */
void timer_init(struct timer *timer, timer_fn_t fn, void *arg);

/*
 * Schedule a timer.
 *
 * The timer callback function is called at or after the given scheduled
 * (absolute) time, in ticks. If the scheduled time denotes the past, the
 * timer function is called immediately.
 *
 * A timer may only be safely scheduled when not already scheduled. When
 * a timer expires and its callback function runs, it is not considered
 * scheduled any more, and may be safely rescheduled from within the
 * callback function. This is how periodic timers are implemented.
 *
 * Note that a timer callback function never runs immediately at its
 * scheduled time. The duration between the actual scheduled time and the
 * time at which the timer callback function runs is called the latency.
 * Ideally, this latency should be as short as possible and never exceed
 * a maximum limit, i.e. be time-bounded. That's what real-time systems
 * are about. Unfortunately, it's quite difficult to achieve most of the
 * time. There are many sources of unexpected latency such as thread
 * scheduling (if not using a real-time scheduling algorithm), priority
 * inversions, other interrupts, cache/TLB misses, contention on the system
 * bus (e.g. when the CPU and a DMA controller compete to become the bus
 * master for a transfer), and memory (DDR SDRAM) access requests being
 * reordered by the controller, to name the most common.
 *
 * Also note that, in addition to latency, another parameter that affects
 * the processing of a timer is resolution. In this implementation, the
 * timer is configured to raise interrupts at the thread scheduler
 * frequency, normally 100 Hz, making the resolution 10ms, which is
 * considered a low resolution. This means that a timer cannot be
 * scheduled to trigger at times that aren't multiples of 10ms on the
 * clock used by the timer system. Finally, note that when scheduling
 * relative timers, unless stated otherwise, the time for the timer to
 * trigger is less than the time requested. This is because the "current
 * time" always marks the past. For example, with the 10ms resolution
 * timer system used here, timer_now() could return 1000 when the "real"
 * current time is actually 1000.9. Assuming the timer is scheduled to
 * trigger at 1001, this means that, instead of waiting a complete tick,
 * the timer would trigger only a tenth of the requested time after being
 * scheduled.
 *
 * What this interface guarantees is that the function never runs before
 * its scheduled time.
 *
 * Finally, for the sake of simplicity, this function doesn't provide a
 * way to cancel a timer.
 */
void timer_schedule(struct timer *timer, unsigned long ticks);

/*
 * Return the scheduled time of a timer, in ticks.
 */
unsigned long timer_get_time(const struct timer *timer);

/*
 * Report a periodic tick to the timer module.
 *
 * This function is called by the hardware timer driver interrupt handler.
 */
void timer_report_tick(void);

#endif /* _TIMER_H */
