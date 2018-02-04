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

#include <stdint.h>

#include "flash.h"

#define FLASH_BASE_ADDR         0x40023c00

#define FLASH_ACR_PRFTEN        0x00000100
#define FLASH_ACR_ICEN          0x00000200
#define FLASH_ACR_DCEN          0x00000400

struct flash_regs {
    uint32_t acr;
    uint32_t keyr;
    uint32_t optkeyr;
    uint32_t sr;
    uint32_t cr;
    uint32_t optcr;
};

static volatile struct flash_regs *flash_regs = (void *)FLASH_BASE_ADDR;

void
flash_setup(void)
{
    /*
     * See 3.5.1 Relation between CPU clock frequency
     * and Flash memory read time.
     */
    flash_regs->acr |= FLASH_ACR_DCEN
                       | FLASH_ACR_ICEN
                       | FLASH_ACR_PRFTEN
                       | 5;
}
