// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
#include <stdint.h>
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"

#include "led.h"
#include "device.h"
#include "log.h"

// Black pill has a single LED on PC13, active low.
// Every nonzero "color" maps to LED on.
void led_rgb(uint32_t hex)
{
    if (hex)
    {
        LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_13);  // on
    }
    else
    {
        LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);    // off
    }
}

void led_setup(void)
{
    // enable GPIOC clock (AHB1 on F4)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void) RCC->AHB1ENR;

    LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_13, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(GPIOC, LL_GPIO_PIN_13, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(GPIOC, LL_GPIO_PIN_13, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(GPIOC, LL_GPIO_PIN_13, LL_GPIO_PULL_NO);

    led_rgb(0);    // off
}

void led_test_colors()
{
    // Should produce blinking of the LED
    int i = 0;
#if DEBUG_LEVEL > 0
    int j = 0;
#endif
    uint32_t time = 0;

    while(1)
    {
        printf1(TAG_GREEN, "%d: %lu\r\n", j++, millis());
        printf1(TAG_GREEN,"blink on/off\r\n");
        time = millis();
        while((millis() - time) < 5000)
        {
            delay(250);
            i = !i;
            led_rgb(i ? 0xffffff : 0);
        }
    }
}
