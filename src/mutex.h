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
 * Mutex module.
 *
 * A mutex is a thread synchronization tool that provides mutual exclusion.
 * The code between locking and unlocking a mutex forms a critical section
 * that only one thread may be running at a time. This is true for all
 * critical sections created with the same mutex, i.e. only one thread may
 * be running code in all the critical sections created with the same mutex.
 *
 * A mutex is initially not owned. When a thread locks a mutex, it becomes
 * owned until unlocked, and any other thread trying to lock the mutex
 * waits for the owner to unlock it. This is called contention. Waiting
 * can be active, e.g. by spinning on the mutex, constantly checking if
 * it's still locked, or passive, by sleeping until the owner unlocks
 * and wakes up one of the waiters. Mutexes are generally passive, and
 * this is also the case for this module.
 *
 * Since mutexes work by blocking execution of some threads, they are a
 * type of lock. Despite their apparent simplicity, they are associated
 * with a number of problems, most considered difficult when compared to
 * other well know problems of computer science.
 *
 * In particular, mutexes are vulnerable to deadlocks, i.e. a situation
 * where all threads involved are waiting for events that cannot occur.
 * Imagine two threads, T1 and T2, and 2 mutexes, M1 and M2. Here is
 * a representation of the execution history of those threads leading to
 * a deadlock :
 *
 * T1 : lock(M1) -> lock(M2) (M2 is locked by T2)
 * T2 : lock(M2) -> lock(M1) (M1 is locked by T1)
 *
 * Here, both threads are waiting for the other to unlock one of the
 * mutexes before unlocking the other mutex, and none can make progress.
 *
 * The most simple solution to avoid deadlocks is to not use locks in
 * the first place, and use other synchronization tools such as disabling
 * preemption, or resorting to lock-free algorithms, but this is often
 * not possible (preemption cannot normally be disabled outside kernel
 * code, and lock-free algorithms are difficult to implement and may
 * lack other dependencies like atomic instructions on small processors).
 *
 * Another simple solution is to avoid holding two mutexes at the same
 * time. It can often be achieved, but not always. Sometimes, holding
 * multiple mutexes at the same time is a requirement. In such cases,
 * the most common solution is to establish a locking order, since a
 * deadlock may only occur if mutexes are acquired in different orders.
 * In the example above, the locking order could be represented as
 * "M1 -> M2", stating that, when hodling both mutexes, M1 must always
 * be locked before M2.
 *
 * Another common problem with mutexes is unbounded priority inversion.
 * Imagine 2 threads, T1 and T2 with priorities 1 and 2 respectively
 * (higher value means higher priority). On a real-time system with
 * a simple fixed priority scheduler, T2 would preempt T1 whenever it
 * is active. But if it then tries to lock a mutex owned by T1, it will
 * have to wait for T1 to unlock the mutex. This is priority inversion,
 * because T1 is running instead of T2 despite having a lower priority.
 * Priority inversion itself is common and often unavoidable in many
 * cases, and if kept reliably short, it normally doesn't disturb
 * real-time behaviour. But if there is no guarantee that the critical
 * section is time-bounded, real-time behaviour may not be achieved.
 * The classic example is three threads, T1, T2, and T3, with priorities
 * 1, 2, and 3 respectively, and T3 locking a mutex already owned by T1 :
 *
 *  time ->
 * T3                   lock(M) +             run ...
 * T2               run +       run +         |
 * T1 lock(M) - run +               unlock(M) +
 * +: preemption                ^^^
 *                              duration not known/bounded
 *
 * Here, T3 has to wait for T1 to unlock the mutex before making progress,
 * but T2 may preempt T1 because of its higher priority. The time T2 runs,
 * keeping T1 from making progress, is likely not known and unbounded, and
 * it indirectly delays T3, despite T3 having a higher priority. This is
 * unbounded priority inversion. It can be avoided at somewhat high cost
 * with priority ceiling/inheritance, or by avoiding the use of mutexes,
 * relying instead on e.g. message queues using preemption for
 * synchronization.
 *
 * The implementation of this module is very simple and doesn't prevent
 * unbounded priority inversions.
 *
 * When deciding whether to use a mutex or to disable preemption for
 * mutual exclusion, keep in mind that all real-world mutex implementations
 * disable preemption internally. As a result, it is best to use mutexes
 * when critical sections are noticeably more expensive than the overhead
 * of locking. Another parameter is whether or not the critical section
 * must remain preemptible.
 *
 * This mutex interface matches the "fast" kind of POSIX mutexes. In
 * particular, a mutex cannot be locked recursively.
 */

#ifndef _MUTEX_H
#define _MUTEX_H

#include <stdbool.h>

#include <lib/list.h>

#include "thread.h"

/*
 * Mutex type.
 *
 * All members are private.
 */
struct mutex {
    struct list waiters;
    struct thread *owner;
    bool locked;
};

/*
 * Initialize a mutex.
 */
void mutex_init(struct mutex *mutex);

/*
 * Lock a mutex.
 *
 * If the given mutex is already locked, the calling thread blocks (by
 * sleeping) and resumes once it holds the mutex.
 */
void mutex_lock(struct mutex *mutex);

/*
 * Try to lock a mutex.
 *
 * This is the non-blocking variant of mutex_lock().
 *
 * Return 0 on success, ERROR_BUSY if locking the mutex failed.
 */
int mutex_trylock(struct mutex *mutex);

/*
 * Unlock a mutex.
 *
 * The calling thread must be the mutex owner.
 */
void mutex_unlock(struct mutex *mutex);

#endif /* _MUTEX_H */
