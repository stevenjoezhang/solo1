// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.

// STM32F411 "Black Pill" configuration (WeAct STM32F411CEU6).
//
// Board facts used below:
//   - HSE crystal 25 MHz (see HSE_CLOCK_HZ if your board differs)
//   - User LED on PC13, active LOW
//   - User KEY on PA0 (black pill: tied to KEY button, pull-up)
//   - USB DM/DP on PA11/PA12
//   - USART1 on PB6/PB7 goes to the on-board linker header pins
#ifndef _APP_H_
#define _APP_H_

#include <stdint.h>
#include "version.h"
#include "solo.h"

// Change this if your board has a different crystal (e.g. 12 MHz clones).
// The PLL setup in init.c derives everything from it.
#define HSE_CLOCK_HZ            25000000u

#define DEBUG_UART              USART1
#define DEBUG_UART_BAUD         115200

// 0 - no debug; 1 - LED+errors on uart; 2 - full trace on uart + CDC console.
// Note: DEBUG > 0 also enables the CDC ACM interface on USB (endpoints 2/3).
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL             0
#endif

#define NON_BLOCK_PRINTING      0

#if defined(SOLO_HACKER)
#define SOLO_PRODUCT_NAME "Solo Hacker " SOLO_VERSION
#else
#define SOLO_PRODUCT_NAME "Solo " SOLO_VERSION
#endif

void printing_init();
// Main hardware init (init.c); called from device.c.
void hw_init(int lf);

#define BOOT_TO_DFU             0

#define ENABLE_U2F              1
#define ENABLE_U2F_EXTENSIONS   1

// LED: single PC13, active low. Any nonzero color = LED on.
// Heartbeat keeps it solid ON while idle ("device alive").
#define LED_INIT_VALUE          0xffffff
#define LED_MAX_SCALER          15
#define LED_MIN_SCALER          1
#define HEARTBEAT_PERIOD        150
// CTAPHID_WINK: blink a few times (any nonzero value = LED on).
#define LED_WINK_VALUE          0x010101

// Button: KEY on PA0, active low (pressed = grounded).
#define SOLO_BUTTON_PORT        GPIOA
#define SOLO_BUTTON_PIN         LL_GPIO_PIN_0

// Set to 1 to accept user presence without the physical button
// (useful for bring-up if your board has no KEY button wired).
#define SKIP_BUTTON_CHECK_WITH_DELAY    0
#define SKIP_BUTTON_CHECK_FAST          0

// AMS/NFC chip is not present on black pill; kept compiled for source
// compatibility, init fails gracefully like a non-NFC Solo.
#define SOLO_AMS_CS_PORT        GPIOB
#define SOLO_AMS_CS_PIN         LL_GPIO_PIN_0
#define SOLO_AMS_IRQ_PORT       GPIOC
#define SOLO_AMS_IRQ_PIN        LL_GPIO_PIN_15

#endif // _APP_H_
