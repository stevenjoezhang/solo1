include ../../fido2/version.mk

CC=$(PREFIX)arm-none-eabi-gcc
CP=$(PREFIX)arm-none-eabi-objcopy
SZ=$(PREFIX)arm-none-eabi-size
AR=$(PREFIX)arm-none-eabi-ar
AS=$(PREFIX)arm-none-eabi-as

# STM32F4 HAL / LL subset.  Note hal_pcd_ex.c is where the OTG FIFO sizing
# functions (HAL_PCDEx_SetTxFiFo/SetRxFiFo) live on F4, and ll_usb.c is the
# DWC2 core driver (the L4 "PMA" USB has no counterpart here).
DRIVER_LIBS := lib/stm32f4xx_hal_pcd.c lib/stm32f4xx_hal_pcd_ex.c lib/stm32f4xx_ll_gpio.c \
       lib/stm32f4xx_ll_rcc.c lib/stm32f4xx_ll_tim.c \
	   lib/stm32f4xx_ll_usb.c lib/stm32f4xx_ll_utils.c lib/stm32f4xx_ll_pwr.c \
	   lib/stm32f4xx_ll_usart.c lib/stm32f4xx_ll_spi.c lib/stm32f4xx_ll_exti.c

USB_LIB := lib/usbd/usbd_cdc.c lib/usbd/usbd_cdc_if.c lib/usbd/usbd_composite.c \
	   lib/usbd/usbd_conf.c lib/usbd/usbd_core.c lib/usbd/usbd_ioreq.c \
       lib/usbd/usbd_ctlreq.c lib/usbd/usbd_desc.c lib/usbd/usbd_hid.c \

VERSION_FULL?=$(SOLO_VERSION_FULL)
VERSION:=$(SOLO_VERSION)
VERSION_MAJ:=$(SOLO_VERSION_MAJ)
VERSION_MIN:=$(SOLO_VERSION_MIN)
VERSION_PAT:=$(SOLO_VERSION_PAT)

VERSION_FLAGS= -DSOLO_VERSION_MAJ=$(VERSION_MAJ) -DSOLO_VERSION_MIN=$(VERSION_MIN) \
	-DSOLO_VERSION_PATCH=$(VERSION_PAT) -DSOLO_VERSION=\"$(VERSION_FULL)\"

_all:
	echo $(SOLO_VERSION_FULL)
	echo $(SOLO_VERSION_MAJ)
	echo $(SOLO_VERSION_MIN)
	echo $(SOLO_VERSION_PAT)

# $< (not $^): objects may carry extra prerequisites (e.g. the DEBUG stamp
# in application.mk) that must not reach the assembler.
%.o: %.s
	$(AS) -o $@ $<
