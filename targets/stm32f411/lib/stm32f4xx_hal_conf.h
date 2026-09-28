#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

/* Only the USB device (PCD) driver is used on this target. */
#define HAL_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

/* Full LL driver (LL_GPIO, LL_RCC, LL_TIM, ...) is enabled on the command
   line (-DUSE_FULL_LL_DRIVER in build/application.mk), not here. */

/* HAL asserts are compiled out. */
#define assert_param(expr) ((void)0U)

/* We do not compile the RCC HAL module; this one query the PCD driver makes
   is provided by a shim in src/init.c.  Declared here so that hal_pcd.c
   sees a prototype. */
uint32_t HAL_RCC_GetHCLKFreq(void);

/* Oscillator values: black pill has a 25 MHz crystal (see HSE_CLOCK_HZ in
   app.h - keep them in sync if your board differs). */
#if !defined  (HSE_VALUE)
  #define HSE_VALUE              25000000U /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */
#if !defined  (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U      /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */
#if !defined  (HSI_VALUE)
  #define HSI_VALUE              16000000U /*!< Value of the Internal oscillator in Hz */
#endif /* HSI_VALUE */
#if !defined  (LSI_VALUE)
 #define LSI_VALUE               32000U    /*!< LSI Typical Value in Hz */
#endif /* LSI_VALUE */
#if !defined  (LSE_VALUE)
  #define LSE_VALUE              32768U    /*!< Value of the External Low Speed oscillator in Hz */
#endif /* LSE_VALUE */
#if !defined  (LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000U     /*!< Time out for LSE start up, in ms */
#endif /* LSE_STARTUP_TIMEOUT */
#if !defined  (EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE     12288000U /*!< Value of the External oscillator in Hz*/
#endif /* EXTERNAL_CLOCK_VALUE */

#define VDD_VALUE                    3300U /*!< Value of VDD in mv */
#define TICK_INT_PRIORITY            0x0FU /*!< tick interrupt priority */

/* Includes ------------------------------------------------------------------*/

#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_PCD_MODULE_ENABLED
  #include "stm32f4xx_hal_pcd.h"
#endif /* HAL_PCD_MODULE_ENABLED */

/* LL drivers used by this target */
#if defined(USE_FULL_LL_DRIVER) || defined(USE_FULL_ASSERT)
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_tim.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_exti.h"
#endif /* USE_FULL_LL_DRIVER */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_HAL_CONF_H */
