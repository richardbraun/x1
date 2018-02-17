/*
 * Copyright (c) 2017-2018 Richard Braun.
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
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <lib/macros.h>
#include <lib/list.h>

#include "cpu.h"
#include "panic.h"
#include "thread.h"

/*
 * List of threads sharing the same priority.
 */
struct thread_list {
    struct list threads;
};

/*
 * Run queue.
 *
 * Threads that aren't in the running state aren't tracked in a run queue.
 * The current thread, which is the one currently running on the processor,
 * isn't in any of the thread lists.
 *
 * A thread may be be "voluntarily" preempted, when it yields the processor
 * itself, or "involuntarily" preempted when an interrupt occurs. Since it's
 * not possible to immediately yield the processor when preemption is
 * disabled (including in the middle of an interrupt handler), preemption
 * may only occur on specific events :
 *  - Calling thread_yield() with preemption enabled.
 *  - Preemption is reenabled, i.e. the preemption level goes back to 0.
 *  - Return from an interrupt.
 *
 * Calling thread_yield() is completely voluntary and is ignored in this
 * discussion. Returning from an interrupt actually reenables preemption,
 * so the only case discussed is reenabling preemption. The mechanism used
 * to trigger scheduling when preemption is reenabled is the yield flag.
 * When the scheduler determines that the current thread should yield the
 * processor, e.g. because an interrupt handler has awaken a higher priority
 * thread, it sets the yield flag of the run queue. When preemption is
 * reenabled, the flag is checked, and if true, the current thread yields
 * the processor.
 *
 * The idle thread runs when no other thread is in the running state.
 *
 * Interrupts and preemption must be disabled when accessing a run queue.
 * Interrupts must be disabled to prevent a timer interrupt from corrupting
 * the run queue. Preemption must be disabled to prevent an interrupt handler
 * from causing an early context switch when returning from interrupt.
 */
struct thread_runq {
    struct thread *current;
    bool yield;
    unsigned int preempt_level;
    unsigned int nr_threads;
    struct thread_list lists[THREAD_NR_PRIORITIES];
    struct thread *idle;
};

enum thread_state {
    THREAD_STATE_RUNNING,   /* Thread is in a run queue */
    THREAD_STATE_SLEEPING,  /* Thread is not running */
    THREAD_STATE_DEAD,      /* Thread is sleeping and may not be awaken */
};

/*
 * Thread structure.
 *
 * The purpose of a thread is to support saving and restoring state in order
 * to achieve time sharing between activities.
 *
 * A thread is mostly used to store the saved state of an activity, called
 * its context. The context is usually made up of some registers that are
 * saved on the stack, the stack itself, and the thread structure.
 * When the context is saved, the stack pointer is updated to the value
 * of the stack register at the time the thread context is saved.
 *
 * Accessing threads is subject to the same synchronization rules as a
 * run queue.
 */
struct thread {
    void *sp;
    enum thread_state state;
    struct list node;
    unsigned int priority;
    struct thread *joiner;
    char name[THREAD_NAME_MAX_SIZE];
    void *stack;
};

/*
 * Run queue singleton.
 */
static struct thread_runq thread_runq;

/*
 * Dummy thread context used to make functions that require a thread context
 * work before the thread module is fully initialized.
 */
static struct thread thread_dummy;

/*
 * Function implementing the idle thread.
 */
static void
thread_idle(void *arg)
{
    (void)arg;

    for (;;) {
        cpu_idle();
    }
}

static void
thread_list_init(struct thread_list *list)
{
    list_init(&list->threads);
}

static void
thread_list_remove(struct thread *thread)
{
    list_remove(&thread->node);
}

static void
thread_list_enqueue_head(struct thread_list *list, struct thread *thread)
{
    list_insert_head(&list->threads, &thread->node);
}

static void
thread_list_enqueue_tail(struct thread_list *list, struct thread *thread)
{
    list_insert_tail(&list->threads, &thread->node);
}

static struct thread *
thread_list_dequeue(struct thread_list *list)
{
    struct thread *thread;

    thread = list_first_entry(&list->threads, typeof(*thread), node);
    thread_list_remove(thread);
    return thread;
}

static bool
thread_list_empty(const struct thread_list *list)
{
    return list_empty(&list->threads);
}

static bool
thread_list_singular(const struct thread_list *list)
{
    return list_singular(&list->threads);
}

static void *
thread_get_stack_pointer(const struct thread *thread)
{
    return thread->sp;
}

static void
thread_set_stack_pointer(struct thread *thread, void *sp)
{
    thread->sp = sp;
}

static bool
thread_is_running(const struct thread *thread)
{
    return thread->state == THREAD_STATE_RUNNING;
}

static void
thread_set_running(struct thread *thread)
{
    thread->state = THREAD_STATE_RUNNING;
}

static void
thread_set_sleeping(struct thread *thread)
{
    thread->state = THREAD_STATE_SLEEPING;
}

static bool
thread_is_dead(const struct thread *thread)
{
    return thread->state == THREAD_STATE_DEAD;
}

static void
thread_set_dead(struct thread *thread)
{
    thread->state = THREAD_STATE_DEAD;
}

static unsigned int
thread_get_priority(struct thread *thread)
{
    return thread->priority;
}

static bool
thread_runq_should_yield(const struct thread_runq *runq)
{
    return runq->yield;
}

static void
thread_runq_set_yield(struct thread_runq *runq)
{
    runq->yield = true;
}

static void
thread_runq_clear_yield(struct thread_runq *runq)
{
    runq->yield = false;
}

static unsigned int
thread_runq_get_preempt_level(const struct thread_runq *runq)
{
    return runq->preempt_level;
}

static void
thread_runq_inc_preempt_level(struct thread_runq *runq)
{
    /*
     * There is no need to protect this increment by disabling interrupts
     * or using a single interrupt-safe instruction such as inc, because
     * the preemption level can only be 0 when other threads access it
     * concurrently. As a result, all threads racing to disable preemption
     * would read 0 and write 1. Although the increment is not atomic, the
     * store is serialized such that the first thread that manages to update
     * memory has effectively prevented all other threads from running,
     * at least until it reenables preemption, which turns the preemption
     * level back to 0, making the read-modify-write increment other threads
     * may have started correct.
     */
    runq->preempt_level++;
    assert(runq->preempt_level != 0);
}

static void
thread_runq_dec_preempt_level(struct thread_runq *runq)
{
    assert(runq->preempt_level != 0);
    runq->preempt_level--;
}

static struct thread_list *
thread_runq_get_list(struct thread_runq *runq, unsigned int priority)
{
    assert(priority < ARRAY_SIZE(runq->lists));
    return &runq->lists[priority];
}

static struct thread *
thread_runq_get_current(struct thread_runq *runq)
{
    return runq->current;
}

static void
thread_runq_put_prev(struct thread_runq *runq, struct thread *thread)
{
    struct thread_list *list;

    if (thread == runq->idle) {
        return;
    }

    list = thread_runq_get_list(runq, thread_get_priority(thread));
    thread_list_enqueue_tail(list, thread);
}

static struct thread *
thread_runq_get_next(struct thread_runq *runq)
{
    struct thread *thread;

    assert(runq->current);

    if (runq->nr_threads == 0) {
        thread = runq->idle;
    } else {
        struct thread_list *list;
        size_t nr_lists;

        nr_lists = ARRAY_SIZE(runq->lists);

        /*
         * Note that size_t is unsigned, which means that when the iterator
         * is 0 and decremented, the new value is actually the maximum value
         * that the type may have, which is necessarily higher than the array
         * size. This behaviour is very well defined by the C specification
         * (6.3.1.3 Signed and unsigned integers), and avoids warnings about
         * mixing types of different signedness.
         */
        for (size_t i = (nr_lists - 1); i < nr_lists; i--) {
            list = thread_runq_get_list(runq, i);

            if (!thread_list_empty(list)) {
                break;
            }
        }

        thread = thread_list_dequeue(list);
    }

    runq->current = thread;
    return thread;
}

static void
thread_runq_add(struct thread_runq *runq, struct thread *thread)
{
    struct thread_list *list;

    assert(!cpu_intr_enabled());
    assert(!thread_preempt_enabled());
    assert(thread_is_running(thread));

    list = thread_runq_get_list(runq, thread_get_priority(thread));
    thread_list_enqueue_head(list, thread);

    runq->nr_threads++;
    assert(runq->nr_threads != 0);

    if (thread_get_priority(thread) > thread_get_priority(runq->current)) {
        thread_runq_set_yield(runq);
    }
}

static void
thread_runq_remove(struct thread_runq *runq, struct thread *thread)
{
    assert(runq->nr_threads != 0);
    runq->nr_threads--;

    assert(!thread_is_running(thread));
    thread_list_remove(thread);
}

static void *
thread_runq_schedule_from_pendsv(struct thread_runq *runq)
{
    struct thread *thread;

    thread = thread_runq_get_current(runq);

    assert(!cpu_intr_enabled());
    assert(runq->preempt_level == 1);

    thread_runq_put_prev(runq, thread);

    if (!thread_is_running(thread)) {
        thread_runq_remove(runq, thread);
    }

    return thread_runq_get_next(runq);
}

static void
thread_runq_schedule(struct thread_runq *runq)
{
    assert(!cpu_intr_enabled());
    assert(runq->preempt_level == 1);

    thread_runq_clear_yield(runq);
}

static void
thread_runq_tick(struct thread_runq *runq)
{
    struct thread_list *list;
    unsigned int priority;

    assert(!cpu_intr_enabled());
    assert(!thread_preempt_enabled());

    priority = runq->current->priority;
    list = thread_runq_get_list(runq, priority);

    if (thread_list_singular(list) && (priority != THREAD_IDLE_PRIORITY)) {
        return;
    }

    thread_runq_set_yield(&thread_runq);
}

static void
thread_yield_if_needed(void)
{
    if (thread_runq_should_yield(&thread_runq)) {
        thread_yield();
    }
}

static unsigned int
thread_preempt_level(void)
{
    return thread_runq_get_preempt_level(&thread_runq);
}

void
thread_preempt_disable(void)
{
    thread_runq_inc_preempt_level(&thread_runq);

    /*
     * This is a compiler barrier. It tells the compiler not to reorder
     * the instructions it emits across this point.
     *
     * Reordering is one of the most effective ways to optimize generated
     * machine code, and the C specification allows compilers to optimize
     * very aggressively, which is why C shouldn't be thought of as a
     * "portable assembler" any more.
     *
     * Sometimes, especially when building critical sections, strong control
     * over ordering is required, and this is when compiler barriers should
     * be used. In this kernel, disabling preemption is one of the primary
     * ways to create critical sections. The following barrier makes sure
     * that no instructions from the critical section leak out before it.
     *
     * When using GCC or a compatible compiler such as Clang, compiler
     * barriers are implemented with an inline assembly expression that
     * includes the "memory" clobber. This clobber tells the compiler that
     * memory may change in unexpected ways. All memory accesses started
     * before the barrier must complete before the barrier, and all memory
     * accesses that complete after the barrier must start after the barrier.
     * See barrier() in macros.h.
     *
     * When calling assembly functions, there is usually no need to add
     * an explicit compiler barrier, because the compiler, not knowing
     * what the external function does, normally assumes memory may change,
     * i.e. that the function has unexpected side effects. In C code however,
     * the compiler has better knowledge about side effects. That knowledge
     * comes from the amount of code in a single compilation unit. The
     * traditional compilation unit when building with optimizations is the
     * source file, but with link-time optimizations (LTO), this can be the
     * whole program. This is why specifying barriers in C code is required
     * for a reliable behaviour.
     *
     * Also note that compiler barriers only affect the machine code
     * generated by the compiler. Processors also internally perform
     * various kinds of reordering. When confined to a single processor,
     * this can safely be ignored because the processor model guarantees
     * that the state seen by software matches program order. This
     * assumption breaks on multiprocessor systems though, where memory
     * barriers are also required to enforce ordering across processors.
     */
    barrier();
}

static void
thread_preempt_enable_no_yield(void)
{
    /* See thread_preempt_disable() */
    barrier();

    thread_runq_dec_preempt_level(&thread_runq);
}

void
thread_preempt_enable(void)
{
    thread_preempt_enable_no_yield();
    thread_yield_if_needed();
}

bool
thread_preempt_enabled(void)
{
    return thread_preempt_level() == 0;
}

uint32_t
thread_preempt_disable_intr_save(void)
{
    /*
     * When disabling both preemption and interrupts, it is best to do it in
     * this order, since, if an interrupt occurs after preemption is disabled
     * but before interrupts are disabled, it may not cause a context switch.
     *
     * Besides, disabling interrupts first would slightly increase interrupt
     * latencies.
     */
    thread_preempt_disable();
    return cpu_intr_save();
}

static void
thread_preempt_enable_no_yield_intr_restore(uint32_t primask)
{
    cpu_intr_restore(primask);
    thread_preempt_enable_no_yield();
}

void
thread_preempt_enable_intr_restore(uint32_t primask)
{
    /*
     * A PendSV exception may only be raised if the preemption level goes
     * back to 0, making it safe to reenable interrupts before.
     */
    cpu_intr_restore(primask);
    thread_preempt_enable();
}

void
thread_enable_scheduler(void)
{
    assert(!cpu_intr_enabled());
    assert(thread_preempt_level() == 1);

    thread_runq_get_next(&thread_runq);

    cpu_intr_enable();

    /* Load the first thread through an SVCall exception */
    cpu_raise_svcall();

    panic("thread: error: unable to load first thread");
}

void
thread_main(thread_fn_t fn, void *arg)
{
    assert(fn);

    assert(cpu_intr_enabled());
    assert(thread_preempt_enabled());

    fn(arg);

    thread_exit();
}

const char *
thread_name(const struct thread *thread)
{
    return thread->name;
}

static void
thread_set_name(struct thread *thread, const char *name)
{
    snprintf(thread->name, sizeof(thread->name), "%s", name);
}

static void
thread_init(struct thread *thread, thread_fn_t fn, void *arg,
            const char *name, char *stack, size_t stack_size,
            unsigned int priority)
{
    /*
     * New threads are created in a state that is similar to preempted threads,
     * since it makes running them for the first time indistinguishable from
     * redispatching a thread that has actually been preempted. This state
     * is very specific and includes the following properties :
     *  - state is running
     *  - interrupts are disabled
     *  - preemption level must be exactly one (see the description of
     *    thread_sleep() in thread.h)
     *
     * A state is artificially forged on the new stack to make it look like
     * the new thread has been preempted.
     */

    if (stack) {
        thread->sp = cpu_stack_forge(stack, stack_size, fn, arg);
    }

    thread->state = THREAD_STATE_RUNNING;
    thread->priority = priority;
    thread->joiner = NULL;
    thread_set_name(thread, name);
    thread->stack = stack;
}

int
thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
              const char *name, size_t stack_size, unsigned int priority)
{
    struct thread *thread;
    uint32_t primask;
    void *stack;

    assert(fn);

    thread = malloc(sizeof(*thread));

    if (!thread) {
        return ENOMEM;
    }

    if (stack_size < THREAD_MIN_STACK_SIZE) {
        stack_size = THREAD_MIN_STACK_SIZE;
    }

    stack = malloc(stack_size);

    if (!stack) {
        free(thread);
        return ENOMEM;
    }

    thread_init(thread, fn, arg, name, stack, stack_size, priority);

    primask = thread_preempt_disable_intr_save();
    thread_runq_add(&thread_runq, thread);
    thread_preempt_enable_intr_restore(primask);

    if (threadp) {
        *threadp = thread;
    }

    return 0;
}

static void
thread_destroy(struct thread *thread)
{
    assert(thread_is_dead(thread));

    free(thread->stack);
    free(thread);
}

void
thread_exit(void)
{
    struct thread *thread;
    uint32_t primask;

    thread = thread_self();

    assert(thread_preempt_enabled());

    primask = thread_preempt_disable_intr_save();

    assert(thread_is_running(thread));
    thread_set_dead(thread);
    thread_wakeup(thread->joiner);
    thread_runq_schedule(&thread_runq);

    thread_preempt_enable_intr_restore(primask);

    cpu_raise_pendsv();

    panic("thread: error: dead thread walking");
}

void
thread_join(struct thread *thread)
{
    uint32_t primask;

    primask = thread_preempt_disable_intr_save();

    thread->joiner = thread_self();

    while (!thread_is_dead(thread)) {
        thread_sleep();
    }

    thread_preempt_enable_intr_restore(primask);

    thread_destroy(thread);
}

struct thread *
thread_self(void)
{
    return thread_runq_get_current(&thread_runq);
}

static struct thread *
thread_create_idle(void)
{
    struct thread *idle;
    void *stack;

    idle = malloc(sizeof(*idle));

    if (!idle) {
        panic("thread: unable to allocate idle thread");
    }

    stack = malloc(THREAD_MIN_STACK_SIZE);

    if (!stack) {
        panic("thread: unable to allocate idle thread stack");
    }

    thread_init(idle, thread_idle, NULL, "idle",
                stack, THREAD_MIN_STACK_SIZE, THREAD_IDLE_PRIORITY);
    return idle;
}

static void
thread_runq_init(struct thread_runq *runq)
{
    /*
     * Set a dummy thread context with preemption disabled to prevent
     * scheduling functions called before the scheduler is running from
     * triggering a context switch.
     */
    thread_init(&thread_dummy, NULL, NULL, "dummy", NULL, 0, 0);
    runq->current = &thread_dummy;
    runq->yield = false;
    runq->preempt_level = 1;
    runq->nr_threads = 0;

    for (size_t i = 0; i < ARRAY_SIZE(runq->lists); i++) {
        thread_list_init(&runq->lists[i]);
    }
}

static void
thread_runq_init_idle(struct thread_runq *runq)
{
    runq->idle = thread_create_idle();
}

void
thread_bootstrap(void)
{
    thread_runq_init(&thread_runq);
}

void
thread_setup(void)
{
    thread_runq_init_idle(&thread_runq);
}

void
thread_yield(void)
{
    uint32_t primask;

    if (!thread_preempt_enabled()) {
        return;
    }

    primask = thread_preempt_disable_intr_save();
    thread_runq_schedule(&thread_runq);
    thread_preempt_enable_no_yield_intr_restore(primask);

    cpu_raise_pendsv();
}

void *
thread_yield_from_svcall(void)
{
    thread_preempt_enable_no_yield();
    return thread_get_stack_pointer(thread_self());
}

void *
thread_yield_from_pendsv(void *prev_sp)
{
    struct thread *thread;
    uint32_t primask;

    primask = thread_preempt_disable_intr_save();

    thread_set_stack_pointer(thread_self(), prev_sp);
    thread = thread_runq_schedule_from_pendsv(&thread_runq);
    thread_preempt_enable_intr_restore(primask);

    return thread_get_stack_pointer(thread);
}

void
thread_sleep(void)
{
    struct thread *thread;
    uint32_t primask;

    thread = thread_self();

    primask = cpu_intr_save();

    assert(thread_is_running(thread));
    thread_set_sleeping(thread);
    thread_runq_schedule(&thread_runq);

    thread_preempt_enable();
    cpu_intr_enable();

    cpu_raise_pendsv();

    cpu_intr_disable();
    thread_preempt_disable();

    assert(thread_is_running(thread));

    cpu_intr_restore(primask);
}

void
thread_wakeup(struct thread *thread)
{
    uint32_t primask;

    if (!thread || (thread == thread_self())) {
        return;
    }

    primask = thread_preempt_disable_intr_save();

    if (!thread_is_running(thread)) {
        assert(!thread_is_dead(thread));
        thread_set_running(thread);
        thread_runq_add(&thread_runq, thread);
    }

    thread_preempt_enable_intr_restore(primask);
}

void
thread_report_tick(void)
{
    thread_runq_tick(&thread_runq);
}
