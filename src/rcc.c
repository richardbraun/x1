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

#include <assert.h>
#include <stdint.h>

#include <lib/macros.h>

#include "rcc.h"

#define RCC_BASE_ADDR           0x40023800

#define RCC_CR_HSION            0x00000001
#define RCC_CR_HSEON            0x00010000
#define RCC_CR_HSERDY           0x00020000
#define RCC_CR_PLLON            0x01000000
#define RCC_CR_PLLRDY           0x02000000

#define RCC_PLLCFGR_PLLM_MASK   0x0000003f
#define RCC_PLLCFGR_PLLN_MASK   0x00007fc0
#define RCC_PLLCFGR_PLLN_SHIFT  6
#define RCC_PLLCFGR_PLLP_MASK   0x00030000
#define RCC_PLLCFGR_PLLP_SHIFT  16
#define RCC_PLLCFGR_PLLSRC_HSE  0x00400000
#define RCC_PLLCFGR_PLLQ_MASK   0x0f000000
#define RCC_PLLCFGR_PLLQ_SHIFT  24

#define RCC_CFGR_SW_PLL         0x00000002
#define RCC_CFGR_SW_MASK        0x00000003
#define RCC_CFGR_SWS_PLL        0x00000008
#define RCC_CFGR_SWS_MASK       0x0000000c
#define RCC_CFGR_PPRE1_MASK     0x00001c00
#define RCC_CFGR_PPRE1_SHIFT    10
#define RCC_CFGR_PPRE2_MASK     0x0000e000
#define RCC_CFGR_PPRE2_SHIFT    13

#define RCC_AHB1ENR_GPIOCEN     0x00000004

#define RCC_APB2RSTR_USART6RST  0x00000020

#define RCC_APB2ENR_USART6EN    0x00000020

struct rcc_regs {
    uint32_t cr;
    uint32_t pllcfgr;
    uint32_t cfgr;
    uint32_t cir;

    uint32_t ahb1rstr;
    uint32_t ahb2rstr;
    uint32_t ahb3rstr;
    uint32_t _reserved0;
    uint32_t apb1rstr;
    uint32_t apb2rstr;
    uint32_t _reserved1;
    uint32_t _reserved2;

    uint32_t ahb1enr;
    uint32_t ahb2enr;
    uint32_t ahb3enr;
    uint32_t _reserved3;
    uint32_t apb1enr;
    uint32_t apb2enr;
    uint32_t _reserved4;
    uint32_t _reserved5;

    uint32_t ahb1lpenr;
    uint32_t ahb2lpenr;
    uint32_t ahb3lpenr;
    uint32_t _reserved6;
    uint32_t apb1lpenr;
    uint32_t apb2lpenr;
    uint32_t _reserved7;
    uint32_t _reserved8;

    uint32_t bdcr;
    uint32_t csr;
    uint32_t _reserved9;
    uint32_t _reserved10;
    uint32_t sscgr;
    uint32_t plli2scfgr;
};

static volatile struct rcc_regs *rcc_regs = (void *)RCC_BASE_ADDR;

static void
rcc_setup_hse(volatile struct rcc_regs *regs)
{
    regs->cr |= RCC_CR_HSEON;

    while (!(regs->cr & RCC_CR_HSERDY));
}

static void
rcc_setup_pll(volatile struct rcc_regs *regs)
{
    uint32_t reg, value;

    reg = regs->pllcfgr;

    reg &= ~RCC_PLLCFGR_PLLM_MASK;
    reg |= RCC_FREQ_HSE / RCC_FREQ_VCO_IN;

    reg &= ~RCC_PLLCFGR_PLLN_MASK;
    reg |= (RCC_FREQ_VCO_OUT / RCC_FREQ_VCO_IN) << RCC_PLLCFGR_PLLN_SHIFT;

    value = ((RCC_FREQ_VCO_OUT / RCC_FREQ_PLLP) / 2) - 1;

    reg &= ~RCC_PLLCFGR_PLLP_MASK;
    reg |= value << RCC_PLLCFGR_PLLP_SHIFT;

    reg &= ~RCC_PLLCFGR_PLLQ_MASK;
    reg |= (RCC_FREQ_VCO_OUT / RCC_FREQ_PLLQ) << RCC_PLLCFGR_PLLQ_SHIFT;

    reg |= RCC_PLLCFGR_PLLSRC_HSE;

    regs->pllcfgr = reg;

    regs->cr |= RCC_CR_PLLON;

    while (!(regs->cr & RCC_CR_PLLRDY));
}

static void
rcc_setup_ahb1(volatile struct rcc_regs *regs)
{
    regs->ahb1enr |= RCC_AHB1ENR_GPIOCEN;
}

static void
rcc_setup_apb1(volatile struct rcc_regs *regs)
{
    uint32_t value;

    value = RCC_FREQ_SYSCLK / RCC_FREQ_APB1;
    assert(ISP2(value));
    value = 0x4 | (__builtin_ffs(value) - 2);

    regs->cfgr &= ~RCC_CFGR_PPRE1_MASK;
    regs->cfgr |= value << RCC_CFGR_PPRE1_SHIFT;
}

static void
rcc_setup_apb2(volatile struct rcc_regs *regs)
{
    uint32_t value;

    value = RCC_FREQ_SYSCLK / RCC_FREQ_APB2;
    assert(ISP2(value));
    value = 0x4 | (__builtin_ffs(value) - 2);

    regs->cfgr &= ~RCC_CFGR_PPRE2_MASK;
    regs->cfgr |= value << RCC_CFGR_PPRE2_SHIFT;

    regs->apb2enr |= RCC_APB2ENR_USART6EN;

    regs->apb2rstr |= RCC_APB2RSTR_USART6RST;
    regs->apb2rstr &= ~RCC_APB2RSTR_USART6RST;
}

static void
rcc_select_sysclk(volatile struct rcc_regs *regs)
{
    regs->cfgr &= ~RCC_CFGR_SW_MASK;
    regs->cfgr |= RCC_CFGR_SW_PLL;

    while ((regs->cfgr & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL);
}

static void
rcc_disable_hsi(volatile struct rcc_regs *regs)
{
    regs->cr &= ~RCC_CR_HSION;
}

void
rcc_setup(void)
{
    rcc_setup_hse(rcc_regs);
    rcc_setup_pll(rcc_regs);
    rcc_setup_ahb1(rcc_regs);
    rcc_setup_apb1(rcc_regs);
    rcc_setup_apb2(rcc_regs);
    rcc_select_sysclk(rcc_regs);
    rcc_disable_hsi(rcc_regs);
}
