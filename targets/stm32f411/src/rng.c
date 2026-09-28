// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
//
// Software entropy source for STM32F411 (no hardware RNG on this chip).
//
// Raw noise: repeated 12-bit conversions of the on-chip temperature sensor
// and Vrefint (no external pin to tamper with), sampled with the longest
// sample time, plus the DWT cycle counter read between conversions.  The
// low bits of those conversions jitter by a few LSB; a SHA-256 based
// conditioner (pool = SHA256(pool || raw samples || UID)) whitens them.
//
// This is NOT a certified TRNG: there is no continuous health-testing
// hardware, and the entropy estimate is informal.  Good enough for a
// hacker-grade DIY authenticator; do not build a production HSM this way.
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx.h"

#include "rng.h"
#include "log.h"
#include "sha256.h"

int __errno = 0;

#define ADC_SAMPLES_PER_BLOCK   64

static uint8_t pool[32];
static uint8_t pool_valid = 0;

static void adc_init_once(void)
{
    static uint8_t inited = 0;
    if (inited)
        return;

    // ADC1 clock: APB2 (48MHz) / 4 = 12MHz (max 36MHz)
    // ADCPRE: 00=/2 01=/4 10=/6 11=/8
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void) RCC->APB2ENR;
    ADC->CCR |= ADC_CCR_ADCPRE_0;   // /4

    // Longest sample time for channels 16 (temp) and 17 (Vrefint)
    // SMPR2 covers channels 10..18; 480 cycles = 0b111.
    ADC1->SMPR2 |= (7U << 18) | (7U << 21); // SMP16 | SMP17

    // Enable internal channels (temp sensor + Vrefint: TSVREFE bit of the
    // ADC *common* register on F4)
    ADC->CCR |= ADC_CCR_TSVREFE;
    // Software trigger, right align (default), single conversion, no DMA
    ADC1->CR2 &= ~(ADC_CR2_EXTSEL | ADC_CR2_EXTEN);
    ADC1->CR1 = 0;

    // Power up
    ADC1->CR2 |= ADC_CR2_ADON;

    inited = 1;
}

// One raw 12-bit conversion of the given channel (16 or 17).
static uint16_t adc_read(uint32_t channel)
{
    // regular sequence: 1 conversion, this channel
    ADC1->SQR1 = 0;
    ADC1->SQR2 = 0;
    ADC1->SQR3 = (channel << 0);

    ADC1->SR = 0;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC))
        ;
    return (uint16_t)(ADC1->DR & 0xfff);
}

static void dwt_init_once(void)
{
    static uint8_t inited = 0;
    if (inited)
        return;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    inited = 1;
}

// Gather one block of whitened bytes into pool.
static void rng_refill(void)
{
    SHA256_CTX ctx;
    uint32_t raw[ADC_SAMPLES_PER_BLOCK];
    uint32_t cycle[ADC_SAMPLES_PER_BLOCK / 2];
    uint32_t uid[3];
    unsigned i;
    uint8_t saw_variety = 0;

    adc_init_once();
    dwt_init_once();

    for (i = 0; i < ADC_SAMPLES_PER_BLOCK; i++)
    {
        // alternate temp / vrefint
        uint16_t v = adc_read((i & 1) ? 17 : 16);
        raw[i] = v ^ (v >> 4);  // fold high nibble in (a bit of it is noise too)
        if (i & 1)
            cycle[i / 2] = DWT->CYCCNT;
        if (i && raw[i] != raw[i - 1])
            saw_variety = 1;
    }

    if (!saw_variety)
    {
        // Stuck ADC? Mix in more timing and UID; log loudly.
        printf2(TAG_ERR, "rng: no ADC variety, using fallback\r\n");
    }

    // chip unique id (96 bits) - constant, but unique per device
    uid[0] = *(volatile uint32_t *)0x1FFF7A10;
    uid[1] = *(volatile uint32_t *)0x1FFF7A14;
    uid[2] = *(volatile uint32_t *)0x1FFF7A18;

    sha256_init(&ctx);
    sha256_update(&ctx, pool, sizeof(pool));
    sha256_update(&ctx, (const uint8_t *)raw, sizeof(raw));
    sha256_update(&ctx, (const uint8_t *)cycle, sizeof(cycle));
    sha256_update(&ctx, (const uint8_t *)uid, sizeof(uid));
    sha256_final(&ctx, pool);
    pool_valid = 1;
}

void rng_get_bytes(uint8_t * dst, size_t sz)
{
    static size_t pool_used = sizeof(pool);
    unsigned int i;

    for (i = 0; i < sz; i++)
    {
        if (pool_used >= sizeof(pool))
        {
            rng_refill();
            pool_used = 0;
        }
        dst[i] = pool[pool_used++];
    }
}

float shannon_entropy(float * p, size_t sz)
{

    unsigned int i;
    float entropy = 0.0f;

    for(i=0; i < sz; i++)
    {
        if (p[i] > 0.0)
        {
            entropy -= p[i] * (float) log( (double) p[i]);
        }
    }

    entropy = entropy / (float) log ((double) 2.0);

    return entropy;
}

// Measure shannon entropy of RNG
float rng_test(size_t n)
{
    unsigned int i;
    int sz = 0;
    uint8_t buf[4];
    int counts[256];
    float p[256];

    memset(counts, 0, sizeof(counts));

    for(i=0; i < n; i+=4)
    {
        rng_get_bytes(buf, 4);
        sz += 4;

        counts[buf[0]]++;
        counts[buf[1]]++;
        counts[buf[2]]++;
        counts[buf[3]]++;
    }

    for (i = 0; i < 256; i++)
    {
        p[i] = ((float)counts[i])/sz;
    }

    return shannon_entropy(p, 256);
}
