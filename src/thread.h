/*
 * Copyright (c) 2017 Richard Braun.
 * Copyright (c) 2017 Jerko Lenstra.
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
 * Thread module.
 *
 * This module provides threads of execution, which encapsulate sequential
 * activities that may be preempted (stopped by the scheduler) so that other
 * threads may run on the processor. This is what makes an operating system
 * "time sharing".
 *
 * Threads are usually scheduled according to a scheduling policy. This
 * module implements a very simple fixed-priority based scheduling policy,
 * where the thread running is ideally always the thread with the highest
 * priority. Because of priority inversions, this isn't always possible.
 * A real-time operating system (RTOS) strives to achieve a behaviour
 * as close as possible to this ideal.
 *
 * Threads may also be called tasks (e.g. in many small embedded RTOS), or
 * lightweight processes (BSDs, Solaris). The word "process", usually refers
 * to much more heavyweight resource containers on Unix, which include both
 * threads and other resources such as an address space and a file descriptor
 * table. Sometimes, the word "task" also refers to this (e.g. in Linux),
 * so using the word "thread" is usually much less ambiguous.
 *
 * Almost all modern operating systems associate a stack with each thread.
 * In the past, threads could explicitely save the state they would need to
 * be restored (look up "continuations"). But since preemptive scheduling
 * requires the ability to preempt a thread at almost any time, a generic
 * way to save and restore the thread state is needed. The most common way
 * is to store that state on the stack, save the stack pointer into the
 * thread structure, and later do the reverse when the thread is dispatched
 * again on the processor.
 *
 * Stack overflows in embedded systems are a major source of bugs. The stack
 * of a thread needs to be large enough to store all the data required by the
 * longest call chain possible and also the saved state of the thread if
 * interrupted right at the end of that chain, something that is quite
 * difficult to determine from static analysis. Some systems provide tools
 * to detect such overflows by e.g. filling the stack with a pattern at
 * creation time, and making sure the pattern hasn't changed near the end
 * of the stack. On systems with virtual memory, guard pages can be used to
 * achieve an even more reliable effect, although at greater cost.
 *
 * A major concept around (kernel) threads is preemption. Disabling preemption
 * means that the current thread may not be preempted, i.e. it will keep
 * running until preemption is reenabled. Therefore, it's not possible for a
 * thread to passively wait for an event if preemption is disabled. Disabling
 * preemption is one of the primary means to implement critical sections.
 * Many embedded systems build critical sections by disabling interrupts,
 * but this lack of decoupling means that interrupts may unnecessarily be
 * disabled, which increases interrupt latency.
 *
 * In X1, interrupts must only be disabled when data is shared between a
 * thread and an interrupt handler, as a means to prevent the interrupt
 * handler from running while the thread is accessing the shared data.
 * Disabling preemption is the preferred way to implement short, time-bounded
 * critical sections. If the cost of the critical section is "too high"
 * (something quite hard to evaluate and which rests entirely on the decision
 * of the developer), mutexes are the recommended tool, since they keep
 * preemption enabled during the critical section.
 */

#ifndef _THREAD_H
#define _THREAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The scheduling frequency is the rate at which the clock used for scheduling
 * ticks. On each tick, the scheduler may mark the currently running thread to
 * yield.
 *
 * This implementation uses periodic ticks, but if the underlying hardware
 * supports reprogramming without losing track of time, a dynamic tick
 * implementation could be used.
 */
#define THREAD_SCHED_FREQ 100

/*
 * Maximum size of thread names, including the null terminating character.
 */
#define THREAD_NAME_MAX_SIZE 16

/*
 * Minimum size of thread stacks.
 */
#define THREAD_MIN_STACK_SIZE 512

/*
 * Total number of thread priorities.
 */
#define THREAD_NR_PRIORITIES    20

/*
 * The lowest priority is used by the idle thread and can be used by very
 * low priority (usually background) threads.
 */
#define THREAD_IDLE_PRIORITY    0

/*
 * Range of regular priorities.
 *
 * Lower values mean lower priorities.
 */
#define THREAD_MIN_PRIORITY     1
#define THREAD_MAX_PRIORITY     (THREAD_NR_PRIORITIES - 1)

/*
 * Type for thread functions.
 */
typedef void (*thread_fn_t)(void *arg);

/*
 * Opaque declaration.
 *
 * An opaque declaration allows the interface to prevent any direct access
 * to the structure itself from external users. The price is that, without
 * the definition, the compiler doesn't know the size of the structure,
 * which prevents allocations from the stack and composition by embedding
 * in other structures, and usually requires the interface to provide
 * dynamic allocation.
 */
struct thread;

/*
 * Early initialization of the thread module.
 *
 * This function initializes a dummy thread context so that functions that
 * require a thread context, such as thread_self(), can be used before the
 * thread module is initialized.
 */
void thread_bootstrap(void);

/*
 * Initialize the thread module.
 *
 * On return, new threads may be created. They will only run once the scheduler
 * is enabled.
 */
void thread_setup(void);

/*
 * Create a thread.
 *
 * The new thread runs the given function, along with the given argument,
 * as soon as permitted by the scheduler.
 *
 * A pointer to the new thread is returned into *threadp, if the latter isn't
 * NULL.
 */
int thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
                  const char *name, size_t stack_size, unsigned int priority);

/*
 * Make the current thread terminate.
 *
 * For the sake of simplicity, this module doesn't provide "detached"
 * threads, where the resources are automatically released on exit.
 * This means that all threads must be joined to avoid resource leaks.
 *
 * This function doesn't return.
 */
void thread_exit(void) __attribute__((noreturn));

/*
 * Wait for a thread to terminate.
 *
 * When the given thread is joined, its resources are released.
 */
void thread_join(struct thread *thread);

/*
 * Return the current thread.
 */
struct thread * thread_self(void);

/*
 * Return the name of the given thread.
 */
const char * thread_name(const struct thread *thread);

/*
 * Yield the processor.
 *
 * The calling thread remains in the running state, and may keep running on
 * the processor if the scheduler determines that it should continue.
 */
void thread_yield(void);

void * thread_yield_from_svcall(void);
void * thread_yield_from_pendsv(void *prev_sp);

/*
 * Make the calling thread sleep until awaken.
 *
 * Preemption must be disabled exactly once before calling this function, and
 * is used to reliably synchronize waiting for/triggering an event.
 *
 * Preemption is used to serialize access to the variables shared between
 * the sleeping thread and the waking code, which may run from another thread
 * (thread context) or from an interrupt handler (interrupt context).
 * If serializing against an interrupt handler, the user must also disable
 * interrupts.
 *
 * By disabling preemption, checking the predicate and sleeping is done
 * atomically with respect to setting the predicate and waking up. Obviously,
 * while sleeping, preemption is reenabled by this function, to allow another
 * thread to set the predicate and wake up the sleeping thread, which is why
 * the preemption level must be exactly one on entry.
 *
 * Note that this function may return for other reasons than the predicate
 * becoming true. These wake-ups are called spurious wake-ups and may be
 * caused by implementation details as well as manually waking up threads
 * (e.g. with C/Unix signals such as SIGINT). This is why sleeping should
 * always be enclosed in a loop, rechecking the predicate on each iteration.
 *
 * Here is an example of sleeping and waking up :
 *
 * static bool predicate;
 * static struct thread *thread;
 *
 * void
 * init(void)
 * {
 *     predicate = false;
 *     thread_create(&thread, run, ...);
 * }
 *
 * void
 * run(void)
 * {
 *     for (;;) {
 *         thread_preempt_disable();    Disable preemption exactly once.
 *
 *         while (!predicate) {         Checking the predicate and sleeping
 *             thread_sleep();          is atomic with respect to setting
 *         }                            the predicate and waking up.
 *
 *         do_something();
 *
 *         thread_preempt_enable();
 *     }
 * }
 *
 * void
 * wakeup(void)
 * {
 *     assert(!thread_preempt_enabled());   Because preemption is disabled,
 *     predicate = true;                    setting the predicate and waking up
 *     thread_wakeup(thread);               is atomic with respect to checking
 *                                          the predicate and sleeping.
 * }
 *
 * This pattern is very close to the POSIX condition variable [1]. The
 * differences are :
 *  - The mutex is replaced by preemption for serialization.
 *  - The condition variable isn't needed because the interface only allows
 *    waking up a single thread.
 *  - Cancellation is completely ignored.
 *
 * You may compare this example with the one in condvar.h for a clear view of
 * the similarities.
 *
 * A mutex would have been difficult to use here since they rely this low
 * level interface, leading to complicated dependency issues. In addition,
 * this is a single processor / single run queue scheduler so using
 * preemption, which is usually processor-local, is sufficient. Finally,
 * it's also the cheapest way to build critical sections, which improves
 * performance.
 *
 * [1] http://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_cond_wait.html.
 */
void thread_sleep(void);

/*
 * Wake up the given thread.
 *
 * For reliable event processing, preemption should be disabled.
 *
 * This function may safely be called from interrupt context.
 */
void thread_wakeup(struct thread *thread);

/*
 * Preemption control functions.
 *
 * Note that enabling preemption actually increments the preemption level
 * whereas disabling preemption decrements it, allowing preemption-based
 * critical sections to nest. Preemption is enabled when the preemption
 * level is 0.
 */
void thread_preempt_enable(void);
void thread_preempt_disable(void);
bool thread_preempt_enabled(void);

/*
 * Preemption control functions which also disable interrupts.
 */
uint32_t thread_preempt_disable_intr_save(void);
void thread_preempt_enable_intr_restore(uint32_t primask);

/*
 * Report a tick.
 *
 * This function must be called from interrupt context.
 */
void thread_report_tick(void);

/*
 * Entry point for new threads.
 */
void thread_main(thread_fn_t fn, void *arg);

/*
 * Enable the scheduler.
 */
void thread_enable_scheduler(void) __attribute__((noreturn));

#endif /* _THREAD_H */
