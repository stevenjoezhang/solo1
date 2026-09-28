// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
//
// STM32F411 initialization (black pill board).
//
// Clock plan:
//   HIGH (USB): HSE (25 MHz default, see HSE_CLOCK_HZ in app.h)
//               -> PLL M=25 N=384 P=4 => SYSCLK 96 MHz
//                                Q=8  => 48 MHz for USB OTG FS
//               AHB  /1 (96 MHz)   flash latency 3 WS
//               APB1 /4 (24 MHz, TIM4 timer clock 2x = 48 MHz -> prescaler 48000 = 1 kHz)
//               APB2 /2 (48 MHz: USART1, SPI1, ADC1)
//   LOW (LF):   HSI 16 MHz direct, APB1/APB2 /1, TIM4 prescaler 16000.
//
// If the HSE crystal does not start (wrong HSE_CLOCK_HZ for your board, or
// no crystal), we fall back to HSI with the same PLL ratios, so USB still
// gets a nominal 48 MHz (within HSI +-1%: usually good enough to enumerate).
#include <stdint.h>
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_tim.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_hal_pcd.h"
#include "stm32f4xx_hal.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_hid.h"
#include "usbd_cdc.h"
#include "usbd_composite.h"
#include "usbd_cdc_if.h"
#include "device.h"
#include "init.h"
#include "sense.h"
#include "led.h"
#include APP_CONFIG

// kHz - TIM4 prescaler for the low-frequency mode (HSI 16 MHz)
#define MAX_CLOCK_RATE      16000

// PLL target: VCO 384 MHz, SYSCLK 96 MHz, USB 48 MHz.
// Works for any integer-MHz HSE: M = HSE_MHz, N = 384, P = 4, Q = 8.
#define PLL_N               384
#define PLL_P               LL_RCC_PLLP_DIV_4
#define PLL_Q               8
#define SYSCLK_HZ           96000000u

#define SET_CLOCK_RATE2()        SystemClock_Config()
#define SET_CLOCK_RATE0()        SystemClock_Config_LF16()
#define SET_CLOCK_RATE1()        SystemClock_Config_LF16()

USBD_HandleTypeDef Solo_USBD_Device;

// Minimal HAL tick machinery (the stock hal.c is not compiled on this
// target; the PCD driver still calls HAL_GetTick for its timeouts).
__IO uint32_t uwTick;
void HAL_IncTick(void)  { uwTick += 1u; }
uint32_t HAL_GetTick(void) { return uwTick; }

// HAL_Delay: the stock implementation (hal.c) is not compiled; SysTick
// provides the 1 ms tick, so spin on HAL_GetTick.
void HAL_Delay(uint32_t Delay)
{
    uint32_t tickstart = HAL_GetTick();
    while ((HAL_GetTick() - tickstart) < Delay)
    {
    }
}

// The PCD driver asks for the HCLK frequency; we do not compile hal_rcc.c,
// so answer from the variable SystemCoreClock (kept up to date by
// LL_SetSystemCoreClock above).
uint32_t HAL_RCC_GetHCLKFreq(void) { return SystemCoreClock; }

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void LL_Init(void);

#define Error_Handler() _Error_Handler(__FILE__,__LINE__)
void _Error_Handler(char *file, int line);

void SystemClock_Config(void);
void SystemClock_Config_LF16(void);

void hw_init(int lowfreq)
{
    LL_Init();
    init_gpio();

    if (lowfreq)
    {
        device_set_clock_rate(DEVICE_LOW_POWER_IDLE);
    }
    else
    {
        SystemClock_Config();
    }

    if (!lowfreq)
    {
        init_pwm();
    }

    init_millisecond_timer(lowfreq);

#if DEBUG_LEVEL > 0
    init_debug_uart();
#endif

    init_rng();

    init_spi();

}

static void LL_Init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    NVIC_SetPriorityGrouping(4);

    /* System interrupt init*/
    NVIC_SetPriority(MemoryManagement_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(DebugMonitor_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
}

void device_set_clock_rate(DEVICE_CLOCK_RATE param)
{
    switch(param)
    {
        case DEVICE_LOW_POWER_IDLE:
            SET_CLOCK_RATE0();
        break;
#if !defined(IS_BOOTLOADER)
        case DEVICE_LOW_POWER_FAST:
            SET_CLOCK_RATE1();
        break;
        case DEVICE_FAST:
            SET_CLOCK_RATE2();
        break;
#endif
    }
}

// Wait for an RCC ready flag with a timeout (no tick available yet, spin).
static int hse_wait_ready(void)
{
    volatile uint32_t t = 0x400000; // ~150 ms worst case at boot
    LL_RCC_HSE_Enable();
    while (!LL_RCC_HSE_IsReady())
    {
        if (!t--)
            return 0;
    }
    return 1;
}

/**
  * @brief System Clock Configuration: HSE(or HSI) -> PLL -> 96 MHz, USB 48 MHz
  */
void SystemClock_Config(void)
{
    uint32_t pllm;
    int have_hse;

    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_PWREN);

    // VCOR max 100MHz @2.7-3.6V on F411 -> use scale 1 (default after reset)
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    have_hse = hse_wait_ready();
    if (have_hse)
    {
        pllm = HSE_CLOCK_HZ / 1000000u;
        LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, pllm, PLL_N, PLL_P);
        LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, pllm, PLL_N, PLL_Q);
    }
    else
    {
        // No crystal: same ratios from the 16 MHz HSI.
        pllm = 16;
        LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, pllm, PLL_N, PLL_P);
        LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, pllm, PLL_N, PLL_Q);
    }

    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1)
    {
    }

    // Bus prescalers before switching SYSCLK
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

    // Flash latency for 96 MHz @3.3V: 3 WS (set *before* speeding up)
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
    if (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_3)
    {
        Error_Handler();
    }

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {
    }

    LL_Init1msTick(SYSCLK_HZ);
    LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
    LL_SetSystemCoreClock(SYSCLK_HZ);

    /* SysTick_IRQn interrupt configuration */
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
}

// Low frequency: HSI 16 MHz, everything /1.  Used briefly at boot and for
// NFC idle paths (this port has no NFC, so mostly the former).
void SystemClock_Config_LF16(void)
{
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_PWREN);

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1)
    {
    }

    // make sure PLL is off before switching away from it
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
    {
    }

    LL_RCC_PLL_Disable();
    while (LL_RCC_PLL_IsReady() == 1)
    {
    }

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
    if (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
    {
        Error_Handler();
    }

    LL_Init1msTick(16000000);
    LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
    LL_SetSystemCoreClock(16000000);

    /* SysTick_IRQn interrupt configuration */
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
}

void init_usb(void)
{
    // USB OTG FS hangs off AHB2 on F4; VBUS sensing is disabled because the
    // black pill does not wire PA9 to VBUS (details in usbd_conf.c).
    SET_BIT(RCC->AHB2ENR, RCC_AHB2ENR_OTGFSEN);

#ifndef IS_BOOTLOADER
    // F411 OTG_FS has only endpoints 0..3, so HID(EP1) + CDC(EP2/3, debug
    // builds only); CCID (needs EP4..6) cannot be supported on this chip.
    USBD_Composite_Set_Classes(&USBD_HID, NULL, &USBD_CDC);
    in_endpoint_to_class[HID_EPIN_ADDR & 0x7F] = 0;
    out_endpoint_to_class[HID_EPOUT_ADDR & 0x7F] = 0;

#if DEBUG_LEVEL > 0
    in_endpoint_to_class[CDC_IN_EP & 0x7F] = 2;
    out_endpoint_to_class[CDC_OUT_EP & 0x7F] = 2;
#endif

    USBD_Init(&Solo_USBD_Device, &Solo_Desc, 0);
    USBD_RegisterClass(&Solo_USBD_Device, &USBD_Composite);
#if DEBUG_LEVEL > 0
    USBD_CDC_RegisterInterface(&Solo_USBD_Device, &USBD_Interface_fops_FS);
#endif
#else
    USBD_Init(&Solo_USBD_Device, &Solo_Desc, 0);
    USBD_RegisterClass(&Solo_USBD_Device, &USBD_HID);
#endif
    USBD_Start(&Solo_USBD_Device);
}

void init_pwm(void)
{
    // no RGB PWM LED on this board; single LED on PC13
    led_setup();
}

void init_debug_uart(void)
{
  LL_USART_InitTypeDef USART_InitStruct;
  LL_GPIO_InitTypeDef GPIO_InitStruct;

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  LL_USART_DeInit(USART1);
  /**USART1 GPIO Configuration
  PB6   ------> USART1_TX
  PB7   ------> USART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6|LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  USART_InitStruct.BaudRate = DEBUG_UART_BAUD;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART1, &USART_InitStruct);

  LL_USART_ConfigAsyncMode(USART1);

  LL_USART_Enable(USART1);
}

void init_gpio(void)
{
  /* GPIO Ports Clock Enable: AHB1 on F4 */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);

  /** USB GPIO Configuration
  PA11  ------> USB_DM   (AF10)
  PA12  ------> USB_DP   (AF10)
  */
  LL_GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11|LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_10;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  LL_GPIO_SetPinMode(SOLO_BUTTON_PORT,SOLO_BUTTON_PIN,LL_GPIO_MODE_INPUT);
  LL_GPIO_SetPinPull(SOLO_BUTTON_PORT,SOLO_BUTTON_PIN,LL_GPIO_PULL_UP);

#ifndef IS_BOOTLOADER
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE0);
  LL_EXTI_InitTypeDef EXTI_InitStruct;
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_0;   // GPIOA_0
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
  LL_EXTI_Init(&EXTI_InitStruct);

  NVIC_EnableIRQ(EXTI0_IRQn);
#endif

}

void init_millisecond_timer(int lf)
{
    LL_TIM_InitTypeDef TIM_InitStruct;

    /* Peripheral clock enable */
    // F411 has no TIM6/DAC (that pairing exists on L4/F407); use TIM4.
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);

    // APB1 = SYSCLK/4, but timers on a prescaled APB1 get 2x the bus clock:
    // TIM4 clock = 48 MHz -> prescaler 48000 = 1 kHz.
    if (!lf)
        TIM_InitStruct.Prescaler = 48000;
    else
        TIM_InitStruct.Prescaler = MAX_CLOCK_RATE;

    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = 90;
    LL_TIM_Init(TIM4, &TIM_InitStruct);

    LL_TIM_DisableARRPreload(TIM4);

    LL_TIM_SetTriggerOutput(TIM4, LL_TIM_TRGO_RESET);

    LL_TIM_DisableMasterSlaveMode(TIM4);

    // enable interrupt
    TIM4->DIER |= 1;

    // Start immediately
    LL_TIM_EnableCounter(TIM4);

    TIM4->SR = 0;
    __enable_irq();
    NVIC_EnableIRQ(TIM4_IRQn);
}

// RNG (ADC based) initializes itself on first use; see rng.c
void init_rng(void)
{
}

/* SPI1 init function (AMS/NFC chip - absent on black pill, init is harmless) */
void init_spi(void)
{
    LL_SPI_InitTypeDef SPI_InitStruct;
    LL_GPIO_InitTypeDef GPIO_InitStruct;

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    /**SPI1 GPIO Configuration
    PA5   ------> SPI1_SCK
    PA6   ------> SPI1_MISO
    PA7   ------> SPI1_MOSI
    */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_5|LL_GPIO_PIN_6|LL_GPIO_PIN_7;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* SPI1 parameter configuration*/
    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
    SPI_InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
    SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;
    SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly = 7;
    LL_SPI_Init(SPI1, &SPI_InitStruct);
}
