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
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include <lib/list.h>
#include <lib/macros.h>

#include "mutex.h"
#include "thread.h"

/*
 * Structure used to bind a waiting thread and a mutex.
 *
 * The purpose of this structure is to avoid adding the node to the thread
 * structure. Instead, it's allocated from the stack and only exists while
 * the thread is waiting for the mutex to be unlocked.
 *
 * When the owner unlocks the mutex, it finds threads to wake up by
 * accessing the mutex list of waiters.
 *
 * Preemption must be disabled when accessing a waiter.
 */
struct mutex_waiter {
    struct list node;
    struct thread *thread;
};

static void
mutex_waiter_init(struct mutex_waiter *waiter, struct thread *thread)
{
    waiter->thread = thread;
}

static void
mutex_waiter_wakeup(struct mutex_waiter *waiter)
{
    thread_wakeup(waiter->thread);
}

void
mutex_init(struct mutex *mutex)
{
    list_init(&mutex->waiters);
    mutex->locked = false;
}

static void
mutex_set_owner(struct mutex *mutex, struct thread *thread)
{
    assert(!mutex->owner);
    assert(!mutex->locked);

    mutex->owner = thread;
    mutex->locked = true;
}

static void
mutex_clear_owner(struct mutex *mutex)
{
    assert(mutex->owner == thread_self());
    assert(mutex->locked);

    mutex->owner = NULL;
    mutex->locked = false;
}

void
mutex_lock(struct mutex *mutex)
{
    struct thread *thread;

    thread = thread_self();

    thread_preempt_disable();

    if (mutex->locked) {
        struct mutex_waiter waiter;

        mutex_waiter_init(&waiter, thread);
        list_insert_tail(&mutex->waiters, &waiter.node);

        do {
            thread_sleep();
        } while (mutex->locked);

        list_remove(&waiter.node);
    }

    mutex_set_owner(mutex, thread);

    thread_preempt_enable();
}

int
mutex_trylock(struct mutex *mutex)
{
    int error;

    thread_preempt_disable();

    if (mutex->locked) {
        error = EBUSY;
    } else {
        error = 0;
        mutex_set_owner(mutex, thread_self());
    }

    thread_preempt_enable();

    return error;
}

void
mutex_unlock(struct mutex *mutex)
{
    struct mutex_waiter *waiter;

    thread_preempt_disable();

    mutex_clear_owner(mutex);

    if (!list_empty(&mutex->waiters)) {
        waiter = list_first_entry(&mutex->waiters, struct mutex_waiter, node);
        mutex_waiter_wakeup(waiter);
    }

    thread_preempt_enable();
}
