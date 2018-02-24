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
 * Condition variable module.
 *
 * A condition variable is a thread synchronization tool used to wait until
 * a predicate, in the form of data shared between multiple threads, becomes
 * true. One or more thread may be waiting on a condition variable for the
 * predicate to become true, and other threads may set the predicate and
 * signal the condition variable, waking up one or all the waiters.
 *
 * This interface of condition variables closely matches the core
 * requirements of the POSIX specification [1]. In particular, a condition
 * variable must always be associated with a mutex, and waiting on a
 * condition variable must always be done in a loop rechecking the
 * predicate to guard against spurious wake-ups, which may occur for
 * various reasons, ranging from implementation details to C/Unix signals,
 * such as SIGINT, sent by a user (this kernel doesn't implement such
 * signals, they're given as a real life example on modern systems).
 *
 * [1] http://pubs.opengroup.org/onlinepubs/9699919799/
 */

#ifndef CONDVAR_H
#define CONDVAR_H

#include <lib/list.h>

#include "mutex.h"

/*
 * Condition variable type.
 *
 * All members are private.
 */
struct condvar {
    struct list waiters;
};

/*
 * Initialize a condition variable.
 */
void condvar_init(struct condvar *condvar);

/*
 * Signal a condition variable.
 *
 * At least one thread is awaken if any threads are waiting on the
 * condition variable.
 *
 * Signalling a condition variable is always safe in the sense that
 * it is permitted and won't make the system crash, but signals may be
 * "missed" if the mutex associated with the condition variable isn't
 * locked when signalling.
 */
void condvar_signal(struct condvar *condvar);

/*
 * Broadcast a condition variable.
 *
 * Same as signalling except all threads waiting on the given condition
 * variable are awaken.
 */
void condvar_broadcast(struct condvar *condvar);

/*
 * Wait on a condition variable.
 *
 * This function makes the calling thread sleep until the given
 * condition variable is signalled. A condition variable is always
 * associated with a mutex when waiting. That mutex is used to
 * serialize access to the variables shared between the waiting
 * thread, and the signalling thread, including the predicate the
 * calling thread is waiting on.
 *
 * When calling this function, the mutex must be locked, so that
 * checking the predicate and waiting on the condition variable is
 * done atomically with respect to signalling. Obviously, while
 * waiting, the mutex must be unlocked, to allow another thread to
 * set the predicate and signal any waiting thread. As a result,
 * this function unlocks the mutex before sleeping, and relocks
 * it before returning.
 *
 * Note that this function may return for other reasons than the
 * condition variable being signalled. These wake-ups are called
 * spurious wake-ups and may be caused by implementation details
 * as well as manually waking up threads (e.g. with C/Unix signals
 * such as SIGINT). This is why waiting on a condition variable
 * should always be enclosed in a loop, rechecking the predicate
 * on each iteration.
 *
 * Here is an example of waiting and signalling :
 *
 * static bool predicate;
 * static struct mutex m;
 * static struct condvar cv;
 *
 * void
 * init(void)
 * {
 *     predicate = false;
 *     mutex_init(&m);
 *     condvar_init(&cv);
 * }
 *
 * void
 * wait(void)
 * {
 *     mutex_lock(&m);
 *
 *     while (!predicate) {             Checking the predicate and waiting
 *         condvar_wait(&cv, &m);       on the condition variable is atomic
 *     }                                with respect to setting the predicate
 *                                      and signalling.
 *     mutex_unlock(&m);
 * }
 *
 * void
 * signal(void)
 * {
 *     mutex_lock(&m);                  Because the mutex is locked, setting
 *     predicate = true;                the predicate and signalling is
 *     condvar_signal(&cv);             atomic with respect to checking the
 *     mutex_unlock(&m);                predicate and waiting on the condition
 *                                      variable.
 * }
 */
void condvar_wait(struct condvar *condvar, struct mutex *mutex);

#endif /* CONDVAR_H */
