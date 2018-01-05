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
 * All references to the Intel 64 and IA-32 Architecture Software Developer's
 * Manual are valid for order number: 325462-061US, December 2016.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/macros.h>

#include "cpu.h"
#include "i8259.h"
#include "thread.h"

/*
 * Segment flags.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 3
 * System Programming Guide :
 *  - 3.4.5 Segment Descriptors
 *  - 3.5 System Descriptor Types
 */
#define CPU_SEG_DATA_RW         0x00000200
#define CPU_SEG_CODE_RX         0x00000900
#define CPU_SEG_S               0x00001000
#define CPU_SEG_P               0x00008000
#define CPU_SEG_DB              0x00400000
#define CPU_SEG_G               0x00800000

#define CPU_IDT_SIZE (CPU_IDT_VECT_IRQ_BASE + I8259_NR_IRQ_VECTORS)

/*
 * Segment descriptor.
 *
 * These entries are found in the GDT and IDT tables (described below).
 * When loading a segment register, the value of the register is a
 * segment selector, which is an index (in bytes) along with flags.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual,
 * Volume 3 System Programming Guide :
 *  - 3.4.2 Segment Selectors
 *  - 3.4.5 Segment Descriptors
 */
struct cpu_seg_desc {
    uint32_t low;
    uint32_t high;
};

/*
 * A pseudo descriptor is an operand for the LGDT/LIDT instructions.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual,
 * Volume 3 System Programming Guide, 3.5.1 Segment Descriptor Tables,
 * Figure 3-11 Pseudo-Descriptor Formats.
 *
 * This structure is packed to prevent any holes between limit and base.
 */
struct cpu_pseudo_desc {
    uint16_t limit;
    uint32_t base;
} __packed;

/*
 * These segment descriptor tables are the Global Descriptor Table (GDT)
 * and the Interrupt Descriptor Table (IDT) respectively. The GDT was
 * historically used to create segments. Segmentation could be used to run
 * multiple instances of the same program at different locations in memory,
 * by changing the base address of segments. It could implement a simple
 * form of memory protection by restricting the length of segments. With
 * modern virtual memory based entirely on paging, segmentation has become
 * obsolete, and all modern systems use a flat memory model, where all
 * segments span the entire physical space. Segments may still be used to
 * provide per-processor or per-thread variables (e.g. this is how TLS,
 * thread-local storage, is implemented).
 *
 * The IDT is used for exception and interrupt handling, collectively known
 * as interrupts. Here, "exception" refers to interrupts originating from
 * the CPU such as a division by zero exception, whereas "IRQ" refers to
 * interrupts raised by external devices. These terms are often used
 * interchangeably. What's important to keep in mind is that interrupts
 * divert the flow of execution of the processor. The IDT tells the processor
 * where to branch when an interrupt occurs.
 *
 * The GDT and IDT should be 8-byte aligned for best performance.
 *
 * See Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 3
 * System Programming Guide :
 *  - 3.5.1 Segment Descriptor Tables (GDT)
 *  - 6.10 Interrupt Descriptor Table (IDT)
 */
static struct cpu_seg_desc cpu_gdt[CPU_GDT_SIZE] __aligned(8);
static struct cpu_seg_desc cpu_idt[CPU_IDT_SIZE] __aligned(8);

/*
 * Handler for external interrupt requests.
 */
struct cpu_irq_handler {
    cpu_irq_handler_fn_t fn;
    void *arg;
};

/*
 * Array where driver IRQ handlers are registered.
 *
 * Interrupts and preemption must be disabled when accessing the handlers.
 */
static struct cpu_irq_handler cpu_irq_handlers[I8259_NR_IRQ_VECTORS];

/*
 * The interrupt frame is the stack content forged by interrupt handlers.
 * They store the data needed to restore the processor to its state prior
 * to the interrupt.
 */
struct cpu_intr_frame {
    /* These members are pushed by the low level ISRs */
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t vector;

    /*
     * This member may be pushed by either the CPU or the low level ISRs
     * for exceptions/interrupts that don't emit such an error code.
     */
    uint32_t error;

    /* These members are automatically pushed by the CPU */
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

/*
 * Declarations for C/assembly functions that are global so that they can
 * be shared between cpu.c and cpu_asm.S, but are considered private to
 * the cpu module.
 */
uint32_t cpu_get_eflags(void);
void cpu_set_eflags(uint32_t eflags);
void cpu_load_gdt(const struct cpu_pseudo_desc *desc);
void cpu_load_idt(const struct cpu_pseudo_desc *desc);
void cpu_intr_main(const struct cpu_intr_frame *frame);

/*
 * Low level interrupt service routines.
 *
 * These are the addresses where the CPU directly branches to when an
 * interrupt is received.
 */
void cpu_isr_divide_error(void);
void cpu_isr_general_protection(void);
void cpu_isr_32(void);
void cpu_isr_33(void);
void cpu_isr_34(void);
void cpu_isr_35(void);
void cpu_isr_36(void);
void cpu_isr_37(void);
void cpu_isr_38(void);
void cpu_isr_39(void);
void cpu_isr_40(void);
void cpu_isr_41(void);
void cpu_isr_42(void);
void cpu_isr_43(void);
void cpu_isr_44(void);
void cpu_isr_45(void);
void cpu_isr_46(void);
void cpu_isr_47(void);

uint32_t
cpu_intr_save(void)
{
    uint32_t eflags;

    eflags = cpu_get_eflags();
    cpu_intr_disable();
    return eflags;
}

void
cpu_intr_restore(uint32_t eflags)
{
    cpu_set_eflags(eflags);
}

bool
cpu_intr_enabled(void)
{
    uint32_t eflags;

    eflags = cpu_get_eflags();
    return eflags & CPU_EFL_IF;
}

void
cpu_halt(void)
{
    cpu_intr_disable();

    for (;;) {
        cpu_idle();
    }
}

static void
cpu_default_intr_handler(void)
{
    printf("cpu: error: unhandled interrupt\n");
    cpu_halt();
}

static void
cpu_seg_desc_init_null(struct cpu_seg_desc *desc)
{
    desc->low = 0;
    desc->high = 0;
}

static void
cpu_seg_desc_init_code(struct cpu_seg_desc *desc)
{
    /*
     * Base: 0
     * Limit: 0xffffffff
     * Privilege level: 0 (most privileged)
     */
    desc->low = 0xffff;
    desc->high = CPU_SEG_G
                 | CPU_SEG_DB
                 | (0xf << 16)
                 | CPU_SEG_P
                 | CPU_SEG_S
                 | CPU_SEG_CODE_RX;
}

static void
cpu_seg_desc_init_data(struct cpu_seg_desc *desc)
{
    /*
     * Base: 0
     * Limit: 0xffffffff
     * Privilege level: 0 (most privileged)
     */
    desc->low = 0xffff;
    desc->high = CPU_SEG_G
                 | CPU_SEG_DB
                 | (0xf << 16)
                 | CPU_SEG_P
                 | CPU_SEG_S
                 | CPU_SEG_DATA_RW;
}

static void
cpu_seg_desc_init_intr_gate(struct cpu_seg_desc *desc,
                            void (*handler)(void))
{
    desc->low = (CPU_GDT_SEL_CODE << 16)
                | (((uint32_t)handler) & 0xffff);
    desc->high = (((uint32_t)handler) & 0xffff0000)
                 | CPU_SEG_P
                 | 0xe00;
}

static void
cpu_pseudo_desc_init(struct cpu_pseudo_desc *desc,
                     const void *addr, size_t size)
{
    assert(size <= 0x10000);
    desc->limit = size - 1;
    desc->base = (uint32_t)addr;
}

static struct cpu_seg_desc *
cpu_get_gdt_entry(size_t selector)
{
    size_t index;

    /*
     * The first 3 bits are the TI and RPL bits
     *
     * See Intel 64 and IA-32 Architecture Software Developer's Manual,
     * Volume 3 System Programming Guide, 3.4.2 Segment Selectors.
     */
    index = selector >> 3;
    assert(index < ARRAY_SIZE(cpu_gdt));
    return &cpu_gdt[index];
}

static void
cpu_irq_handler_init(struct cpu_irq_handler *handler)
{
    handler->fn = NULL;
}

static struct cpu_irq_handler *
cpu_lookup_irq_handler(unsigned int irq)
{
    assert(irq < ARRAY_SIZE(cpu_irq_handlers));
    return &cpu_irq_handlers[irq];
}

static void
cpu_irq_handler_set_fn(struct cpu_irq_handler *handler,
                       cpu_irq_handler_fn_t fn, void *arg)
{
    assert(handler->fn == NULL);
    handler->fn = fn;
    handler->arg = arg;
}

static void
cpu_setup_gdt(void)
{
    struct cpu_pseudo_desc pseudo_desc;

    cpu_seg_desc_init_null(cpu_get_gdt_entry(CPU_GDT_SEL_NULL));
    cpu_seg_desc_init_code(cpu_get_gdt_entry(CPU_GDT_SEL_CODE));
    cpu_seg_desc_init_data(cpu_get_gdt_entry(CPU_GDT_SEL_DATA));

    cpu_pseudo_desc_init(&pseudo_desc, cpu_gdt, sizeof(cpu_gdt));
    cpu_load_gdt(&pseudo_desc);
}

static void
cpu_setup_idt(void)
{
    struct cpu_pseudo_desc pseudo_desc;

    for (size_t i = 0; i < ARRAY_SIZE(cpu_irq_handlers); i++) {
        cpu_irq_handler_init(cpu_lookup_irq_handler(i));
    }

    for (size_t i = 0; i < ARRAY_SIZE(cpu_idt); i++) {
        cpu_seg_desc_init_intr_gate(&cpu_idt[i], cpu_default_intr_handler);
    }

    cpu_seg_desc_init_intr_gate(&cpu_idt[CPU_IDT_VECT_DIV],
                                cpu_isr_divide_error);
    cpu_seg_desc_init_intr_gate(&cpu_idt[CPU_IDT_VECT_GP],
                                cpu_isr_general_protection);
    cpu_seg_desc_init_intr_gate(&cpu_idt[32], cpu_isr_32);
    cpu_seg_desc_init_intr_gate(&cpu_idt[33], cpu_isr_33);
    cpu_seg_desc_init_intr_gate(&cpu_idt[34], cpu_isr_34);
    cpu_seg_desc_init_intr_gate(&cpu_idt[35], cpu_isr_35);
    cpu_seg_desc_init_intr_gate(&cpu_idt[36], cpu_isr_36);
    cpu_seg_desc_init_intr_gate(&cpu_idt[37], cpu_isr_37);
    cpu_seg_desc_init_intr_gate(&cpu_idt[38], cpu_isr_38);
    cpu_seg_desc_init_intr_gate(&cpu_idt[39], cpu_isr_39);
    cpu_seg_desc_init_intr_gate(&cpu_idt[40], cpu_isr_40);
    cpu_seg_desc_init_intr_gate(&cpu_idt[41], cpu_isr_41);
    cpu_seg_desc_init_intr_gate(&cpu_idt[42], cpu_isr_42);
    cpu_seg_desc_init_intr_gate(&cpu_idt[43], cpu_isr_43);
    cpu_seg_desc_init_intr_gate(&cpu_idt[44], cpu_isr_44);
    cpu_seg_desc_init_intr_gate(&cpu_idt[45], cpu_isr_45);
    cpu_seg_desc_init_intr_gate(&cpu_idt[46], cpu_isr_46);
    cpu_seg_desc_init_intr_gate(&cpu_idt[47], cpu_isr_47);

    cpu_pseudo_desc_init(&pseudo_desc, cpu_idt, sizeof(cpu_idt));
    cpu_load_idt(&pseudo_desc);
}

static void
cpu_print_frame(const struct cpu_intr_frame *frame)
{
    printf("cpu: vector: %-8x eip: %08x eax: %08x ebx: %08x\n"
           "cpu:  error: %-8x esp: %08x ecx: %08x edx: %08x\n"
           "cpu: eflags: %08x ebp: %08x esi: %08x edi: %08x\n",
           (unsigned int)frame->vector, (unsigned int)frame->eip,
           (unsigned int)frame->eax, (unsigned int)frame->ebx,
           (unsigned int)frame->error, (unsigned int)(frame + 1),
           (unsigned int)frame->ecx, (unsigned int)frame->edx,
           (unsigned int)frame->eflags, (unsigned int)frame->ebp,
           (unsigned int)frame->esi, (unsigned int)frame->edi);
}

static void
cpu_exc_main(const struct cpu_intr_frame *frame)
{
    printf("cpu: exception:\n");
    cpu_print_frame(frame);

    switch (frame->vector)
    {
    case CPU_IDT_VECT_DIV:
        panic("cpu: divide error");
    case CPU_IDT_VECT_GP:
        panic("cpu: general protection fault");
    default:
        cpu_default_intr_handler();
    }
}

void
cpu_intr_main(const struct cpu_intr_frame *frame)
{
    struct cpu_irq_handler *handler;
    unsigned int irq;

    assert(!cpu_intr_enabled());
    assert(frame->vector < ARRAY_SIZE(cpu_idt));

    /*
     * Interrupt handlers may call functions that may in turn yield the
     * processor. When running in interrupt context, as opposed to thread
     * context, there is no way to yield the processor, because the context
     * isn't saved into a scheduled structure, which is what threads are
     * for. As a result, disable preemption to prevent an invalid context
     * switch.
     */
    thread_preempt_disable();

    if (frame->vector < CPU_IDT_VECT_IRQ_BASE) {
        cpu_exc_main(frame);
    } else {
        irq = frame->vector - CPU_IDT_VECT_IRQ_BASE;

        /*
         * Acknowledge the IRQ as early as possible to allow another one to
         * be raised.
         */
        i8259_irq_eoi(irq);

        handler = cpu_lookup_irq_handler(irq);

        if (!handler || !handler->fn) {
            printf("cpu: error: invalid handler for irq %u\n", irq);
        } else {
            handler->fn(handler->arg);
        }
    }

    /*
     * On entry, preemption could have been either enabled or disabled.
     * If it was enabled, this call will reenable it. As a side effect,
     * it will check if the current thread was marked for yielding, e.g.
     * because the interrupt handler has awaken a higher priority thread,
     * in which case a context switch is triggerred. Such context switches
     * are called involuntary.
     *
     * Here is what the stack looks like when such a context switch occurs :
     *
     * |                                 | Stack grows down.
     * |                                 |
     * | stack of the interrupted thread |
     * |                                 |
     * +---------------------------------+ <- interrupt occurs
     * |                                 |
     * | struct cpu_intr_frame           |
     * |                                 |
     * +---------------------------------+
     * |                                 |
     * | cpu_intr_main stack frame       |
     * |                                 |
     * +---------------------------------+
     * |                                 |
     * | thread function stack frames    |
     * |                                 |
     * +---------------------------------+
     * |                                 |
     * | thread context on switch        | See thread_switch_context in
     * |                                 | thread_asm.S.
     * +---------------------------------+
     */
    thread_preempt_enable();
}

void
cpu_irq_register(unsigned int irq, cpu_irq_handler_fn_t fn, void *arg)
{
    struct cpu_irq_handler *handler;
    uint32_t eflags;

    thread_preempt_disable();
    eflags = cpu_intr_save();

    handler = cpu_lookup_irq_handler(irq);
    cpu_irq_handler_set_fn(handler, fn, arg);
    i8259_irq_enable(irq);

    thread_preempt_disable();
    cpu_intr_restore(eflags);
}

void
cpu_setup(void)
{
    cpu_setup_gdt();
    cpu_setup_idt();
}
