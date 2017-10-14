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
 * Intel 8259 programmable interrupt controller (PIC) driver.
 */

#ifndef _I8259_H
#define _I8259_H

/*
 * Range of vectors used for IRQ handling, 8 per PIC.
 */
#define I8259_NR_IRQ_VECTORS 16

/*
 * Initialize the i8259 module.
 */
void i8259_setup(void);

/*
 * Enable an IRQ line on the PIC.
 */
void i8259_irq_enable(unsigned int irq);

/*
 * Disable an IRQ line on the PIC.
 */
void i8259_irq_disable(unsigned int irq);

/*
 * Report an end of interrupt.
 *
 * This function must be called with interrupts disabled.
 */
void i8259_irq_eoi(unsigned int irq);

#endif /* _I8259_H */
