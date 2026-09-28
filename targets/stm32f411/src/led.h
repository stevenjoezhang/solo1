// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>

void led_setup(void);
void led_rgb(uint32_t hex);
void led_test_colors();

// Black pill: single LED on PC13, active low.
#define LED_PIN       LL_GPIO_PIN_13
#define LED_PORT      GPIOC

#endif
