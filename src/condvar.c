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

#include <stdbool.h>

#include <lib/list.h>

#include "condvar.h"
#include "mutex.h"
#include "thread.h"

/*
 * Structure used to bind a waiting thread and a condition variable.
 *
 * The purpose of this structure is to avoid adding the node to the thread
 * structure. Instead, it's allocated from the stack and only exists while
 * the thread is waiting for the condition variable to be signalled.
 *
 * When another thread signals the condition variable, it finds threads
 * to wake up by accessing the condition variable list of waiters.
 *
 * The awaken member records whether the waiting thread has actually been
 * awaken, to guard against spurious wake-ups.
 *
 * Preemption must be disabled when accessing a waiter.
 */
struct condvar_waiter {
    struct list node;
    struct thread *thread;
    bool awaken;
};

static void
condvar_waiter_init(struct condvar_waiter *waiter, struct thread *thread)
{
    waiter->thread = thread;
    waiter->awaken = false;
}

static bool
condvar_waiter_awaken(const struct condvar_waiter *waiter)
{
    return waiter->awaken;
}

static bool
condvar_waiter_wakeup(struct condvar_waiter *waiter)
{
    if (condvar_waiter_awaken(waiter)) {
        return false;
    }

    thread_wakeup(waiter->thread);
    waiter->awaken = true;
    return true;
}

void
condvar_init(struct condvar *condvar)
{
    list_init(&condvar->waiters);
}

void
condvar_signal(struct condvar *condvar)
{
    struct condvar_waiter *waiter;
    bool awaken;

    thread_preempt_disable();

    list_for_each_entry(&condvar->waiters, waiter, node) {
        awaken = condvar_waiter_wakeup(waiter);

        if (awaken) {
            break;
        }
    }

    thread_preempt_enable();
}

void
condvar_broadcast(struct condvar *condvar)
{
    struct condvar_waiter *waiter;

    /*
     * Note that this broadcast implementation, a very simple and naive one,
     * allows a situation known as the "thundering herd problem" [1].
     *
     * Remember that a condition variable is always associated with a mutex
     * when waiting on it. This means that, when broadcasting a condition
     * variable on which many threads are waiting, they will all be awaken,
     * but only one of them acquires the associated mutex. All the others
     * will sleep, waiting for the mutex to be unlocked. This unnecessary
     * round of wake-ups closely followed by sleeps may be very expensive
     * compared to the cost of the critical sections around the wait, and
     * that cost increases linearly with the number of waiting threads.
     *
     * Smarter but more complicated implementations can avoid this problem,
     * e.g. by directly queuing the current waiters on the associated mutex.
     *
     * [1] https://en.wikipedia.org/wiki/Thundering_herd_problem
     */

    thread_preempt_disable();

    list_for_each_entry(&condvar->waiters, waiter, node) {
        condvar_waiter_wakeup(waiter);
    }

    thread_preempt_enable();
}

void
condvar_wait(struct condvar *condvar, struct mutex *mutex)
{
    struct condvar_waiter waiter;
    struct thread *thread;

    thread = thread_self();
    condvar_waiter_init(&waiter, thread);

    thread_preempt_disable();

    /*
     * Unlocking the mutex associated with the condition variable after
     * acquiring the condition variable (done here by disabling preemption)
     * is what makes waiting "atomic". Note that atomicity isn't absolute.
     * Here, the wait is atomic with respect to concurrent signals.
     *
     * Signalling a condition variable is always safe in the sense that
     * it is permitted and won't make the system crash, but signals may be
     * "missed" if the associated mutex isn't locked when signalling.
     */
    mutex_unlock(mutex);

    list_insert_tail(&condvar->waiters, &waiter.node);

    do {
        thread_sleep();
    } while (!condvar_waiter_awaken(&waiter));

    list_remove(&waiter.node);

    thread_preempt_enable();

    /*
     * Unlike releasing the mutex earlier, relocking the mutex may be
     * done before or after releasing the condition variable. In this
     * case, it may not be done before because acquiring the condition
     * variable is achieved by disabling preemption, which forbids
     * sleeping, and therefore locking a mutex, but another implementation
     * may use a different synchronization mechanism.
     *
     * It's also slightly better to relock outside the previous critical
     * section in order to make it shorter.
     */
    mutex_lock(mutex);
}
