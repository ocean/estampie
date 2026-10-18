/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 SARADC: PB6 = MASTER pot (ch4),
 * PB1 = battery divider (ch3). Polled, 10 bit, from the main loop.
 * ESTAMPIE (KEY TOUCH, after Charles Vestal's Velo and MONK-1, github.com/charlesvestal/fm1-velocity and fm1-monk,
 * GPL-3.0-only): two of the key matrix rows are converter inputs too, PA7 (ch0) and PA8 (ch1), and with KEY TOUCH on
 * the scan tick owns the converter (hal/fm1_input.h fm1_an: the rows, the pot and the battery between two ticks), so
 * the main loop reads fm1_an.master / fm1_an.batt then, never fm1_adc_read.
 *
 *   fm1_adc_init()        both pins analog inputs, no pulls
 *   fm1_adc_read(ch)      0..1023, -1 on timeout
 *   fm1_adc_start(ch, cfg), fm1_adc_done(), fm1_adc_take(), fm1_adc_abort()   one conversion, not waited for;
 *                         cfg = FM1_ADC_CFG(div, delay): a conversion short enough to fit in a 100 us scan tick
 *
 * CON: bits 0-2 the clock divider (fm1_adc_read: 6), bit 3 set, bit 4 enable, bit 5 interrupt enable, bit 6 start /
 * clear pending, bit 7 pending, bits 8-11 the channel, bits 12-15 the start delay in 8 converter clocks (15). Velo
 * measured divider 7, delay 2 as the fastest that reads true (faster returns the rails).
 */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"

#define FM1_ADC_CON (*(volatile uint32_t *)0x13100u)
#define FM1_ADC_RES (*(volatile uint32_t *)0x13104u)
#define FM1_WLA_CON0 (*(volatile uint32_t *)0x11900u)
enum { FM1_ADC_ROW_PA7 = 0, FM1_ADC_ROW_PA8 = 1, FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
#define FM1_ADC_CFG(div, delay) ((uint32_t)((div) & 7u) | 8u | ((uint32_t)((delay) & 15u) << 12))
#define FM1_ADC_CFG_FAST FM1_ADC_CFG(7, 2)         /* (Velo's: the fastest that reads true) */

/* Both are only ever analog inputs: PB1 held low for 8 s would reset the
 * chip (isd_config). */
static void fm1_adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);
    FM1_PR(FM1_PB, FM1_DIE) &= ~m;
    FM1_PR(FM1_PB, FM1_PU) &= ~m;
    FM1_PR(FM1_PB, FM1_PD) &= ~m;
    FM1_PR(FM1_PB, FM1_DIR) |= m;
}

static int32_t fm1_adc_read(uint32_t ch)
{
    uint32_t t, v;
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0xF04Eu | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (FM1_ADC_CON & 0x80u)
            break;
    v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}

static void fm1_adc_start(uint32_t ch, uint32_t cfg)
{
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = (cfg & 0xF00Fu) | 0x40u | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
}
static inline int fm1_adc_done(void) { return (FM1_ADC_CON & 0x80u) != 0; }
static inline uint32_t fm1_adc_take(void)
{
    uint32_t v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return v;
}
static inline void fm1_adc_abort(void)
{
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
}
