# solo1 firmware for STM32F411 (WeAct black pill).
#
# There is no bootloader on this target: the application lives at
# 0x08000000 and is flashed over USB DFU (hold BOOT0, press reset) or via
# SWD.  See README.md in this directory.

ifndef DEBUG
DEBUG=0
endif

VERSION_FULL?=$(shell git describe)
APPMAKE=build/application.mk VERSION_FULL=${VERSION_FULL}

# The build compiles objects next to the shared sources (../../fido2, ../../crypto),
# so objects left by a different target (e.g. targets/stm32l432) would be silently
# linked in here.  Two triggers force a clean: the stamp records a different chip,
# or shared objects are newer than the stamp (i.e. some other build recompiled
# them since our last build).
CHIP_STAMP=.built_chip
CHIP=STM32F411xE

define ensure_chip
	if [ -f $(CHIP_STAMP) ] && { [ "`cat $(CHIP_STAMP)`" != "$(CHIP)" ] || \
	     [ -n "`find ../../fido2 ../../crypto -path '*/salty' -prune -o -name '*.o' -newer $(CHIP_STAMP) -print -quit 2>/dev/null`" ]; }; then \
		echo "*** shared objects are stale (built for `cat $(CHIP_STAMP)`); cleaning ***"; \
		$(MAKE) -f $(APPMAKE) clean; \
	fi
endef

.PHONY: all all-hacker firmware firmware-debug-1 firmware-debug-2 flash_dfu cbor salty clean

# Default: release firmware (HID only, no debug output).
all all-hacker firmware:
	@$(ensure_chip)
	$(MAKE) -f $(APPMAKE) -j8 solo.hex PREFIX=$(PREFIX) DEBUG=0
	@echo $(CHIP) > $(CHIP_STAMP)

# Debug builds add the CDC ACM console on endpoints 2/3 + UART logging.
firmware-debug-1:
	@$(ensure_chip)
	$(MAKE) -f $(APPMAKE) -j8 solo.hex PREFIX=$(PREFIX) DEBUG=1
	@echo $(CHIP) > $(CHIP_STAMP)

firmware-debug-2:
	@$(ensure_chip)
	$(MAKE) -f $(APPMAKE) -j8 solo.hex PREFIX=$(PREFIX) DEBUG=2
	@echo $(CHIP) > $(CHIP_STAMP)

# Flash via the ST system-memory DFU bootloader (board in BOOT0=1 mode):
#   dfu-util -a 0 -d 0483:df11 -D solo.hex
flash_dfu:
	dfu-util -a 0 -d 0483:df11 -D solo.hex

cbor:
	$(MAKE) -f $(APPMAKE) cbor PREFIX=$(PREFIX) DEBUG=0

salty:
	$(MAKE) -f $(APPMAKE) salty

clean:
	$(MAKE) -f $(APPMAKE) clean
