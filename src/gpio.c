/*
 * Copyright (c) 2018 Richard Braun.
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
#include <stdint.h>

#include "gpio.h"

#define GPIO_C_BASE_ADDR 0x40020800

struct gpio_regs {
    uint32_t moder;
    uint32_t otyper;
    uint32_t ospeedr;
    uint32_t pupdr;
    uint32_t idr;
    uint32_t odr;
    uint32_t bsrr;
    uint32_t lckr;
    uint32_t afrl;
    uint32_t afrh;
};

static volatile struct gpio_regs *gpio_c_regs = (void *)GPIO_C_BASE_ADDR;

static void
gpio_compute_location(unsigned int io, unsigned int nr_bits,
                      uint32_t *shift, uint32_t *mask)
{
    *shift = (io * nr_bits);
    *mask = ((1 << nr_bits) - 1) << *shift;
}

static void
gpio_set_af(volatile struct gpio_regs *regs, unsigned int io,
            uint32_t af, uint32_t speed, uint32_t pupd)
{
    uint32_t shift, mask, value;
    volatile uint32_t *reg;

    gpio_compute_location(io, 2, &shift, &mask);
    value = (af == 15) ? 1 : 2;
    regs->moder &= ~mask;
    regs->moder |= value << shift;

    gpio_compute_location(io, 2, &shift, &mask);
    regs->ospeedr &= ~mask;
    regs->ospeedr |= speed << shift;

    gpio_compute_location(io, 2, &shift, &mask);
    regs->pupdr &= ~mask;
    regs->pupdr |= pupd << shift;

    if (io < 8) {
        reg = &regs->afrl;
    } else {
        reg = &regs->afrh;
        io -= 8;
    }

    gpio_compute_location(io, 4, &shift, &mask);
    *reg &= ~mask;
    *reg |= af << shift;
}

static void
gpio_set_output(volatile struct gpio_regs *regs, unsigned int io, bool high)
{
    uint32_t shift, mask;

    gpio_compute_location(io, 1, &shift, &mask);

    if (high) {
        regs->odr |= high << shift;
    } else {
        regs->odr &= ~mask;
    }
}

void
gpio_setup(void)
{
    gpio_set_af(gpio_c_regs, 6, 8, 1, 1);   /* UART6 TX */
    gpio_set_af(gpio_c_regs, 7, 8, 1, 1);   /* UART6 RX */
    gpio_set_af(gpio_c_regs, 13, 15, 0, 0); /* LED */
}

void
gpio_led_on(void)
{
    gpio_set_output(gpio_c_regs, 13, false);
}

void
gpio_led_off(void)
{
    gpio_set_output(gpio_c_regs, 13, true);
}
