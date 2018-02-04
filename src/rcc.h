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

#ifndef _RCC_H
#define _RCC_H

#include "cpu.h"

#define RCC_FREQ_SYSCLK         CPU_FREQ
#define RCC_FREQ_HSE            12000000
#define RCC_FREQ_VCO_IN         2000000
#define RCC_FREQ_VCO_OUT        336000000
#define RCC_FREQ_PLLP           RCC_FREQ_SYSCLK
#define RCC_FREQ_PLLQ           48000000
#define RCC_FREQ_APB1           42000000
#define RCC_FREQ_APB2           84000000

/*
 * Initialize the rcc module.
 */
void rcc_setup(void);

#endif /* _RCC_H */
