# solo1 for STM32F411 (WeAct "black pill" STM32F411CEU6)

A CTAP2 / FIDO2 / U2F hardware authenticator for the common black pill
board.  This is a hacker-grade DIY build: it compiles and is statically
verified, but has never been certified (see *Limitations*).

## Hardware

| Function | Pin | Notes |
|---|---|---|
| USB D-/D+ | PA11 / PA12 | OTG_FS, no VBUS sensing (black pill doesn't wire PA9 to VBUS) |
| Button ("touch") | PA0 | on-board KEY, active low, pull-up |
| LED | PC13 | on-board, active low; solid ON = running, blink = activity |
| Debug UART | PB6 (TX) / PB7 (RX) | 115200 8N1, DEBUG_LEVEL > 0 builds only |
| HSE | 25 MHz | `HSE_CLOCK_HZ` in `src/app.h` — change for 12 MHz clones |

NFC (AMS) and CCID are not present on this board/chip (OTG_FS has only
endpoints 0–3); the NFC code is compiled out via stubs, and the USB
composite is HID + CDC (debug builds only).

## Flash layout

The STM32F411 has non-uniform sectors (4x16K, 1x64K, 3x128K).  The port
keeps solo's logical 2 KB page scheme and places the 15 live data pages at
the very end of the 128 KB sector 6, so the fido2/ layer's `flash_addr()`
mapping is unchanged:

```
0x08000000  sectors 0-5 (256K)   application (pages 0..127)
0x08040000  sector  6  (128K)    solo data pages 177..191
              page 177  attestation cert
              pages 178-187  resident keys (RK)
              page 188  COUNTER2   page 189  COUNTER1
              page 190  STATE2     page 191  STATE1
0x08060000  sector  7            unused
```

There is **no bootloader** on this target: the application starts at
`0x08000000` and boots directly.  `boot_solo_bootloader()` jumps to the ST
system-memory DFU bootloader at `0x1FFF0000` instead.

Sector 6 is erased as a whole, so `flash_erase_page()` stages the other 14
pages in a 30 KB static RAM buffer and writes them back afterwards, in
fault-priority order (counters first).  Consequences:

* an erase takes ~1-2 s during which the device is USB-deaf (some hosts
  may complain about a slow transaction — retry);
* if power is lost mid-erase/write-back, data pages can be lost.  The
  firmware recovers state from the backup page pair, but resident keys
  written since the last successful write may be gone.

## Clocking

HSE 25 MHz -> PLL (M=25, N=384, P=4) -> 96 MHz SYSCLK, Q=8 -> 48 MHz USB.
APB1 /4, APB2 /2, flash 3 wait states.  If the HSE doesn't start (wrong
crystal value), it falls back to HSI with identical ratios, so USB still
enumerates (nominally within HSI tolerance).  The 1 ms timebase is TIM4
(the F411 has no TIM6/DAC, unlike the L4) on the APB1 timer clock.

The RNG is **software**: ADC temp-sensor/Vrefint LSB jitter + DWT cycle
counter + chip UID, conditioned through SHA-256.  Not a certified TRNG —
fine for a personal hacker key, not for a production HSM.

## Building

Cross toolchain `arm-none-eabi-gcc`, Rust/Cargo (with the
`thumbv7em-none-eabihf` target) and `python3` on PATH:

```
cd targets/stm32f411
make salty cbor      # once, and after cargo/tinycbor changes
make firmware        # release: HID only
make firmware-debug-2  # + CDC ACM console and UART logging
```

Output: `solo.hex` (~82 KB release).

**Warning about object sharing:** like the other solo targets, objects are
compiled next to the shared sources in `../../fido2` and `../../crypto`.
Building `targets/stm32l432` and this target in the same tree silently
mixes objects unless you clean in between.  This target's Makefile detects
that (`.built_chip` stamp) and cleans automatically; the stock l432
Makefile does not, so if you switch l432 -> f411 always go through this
Makefile (`make firmware` here), not a bare
`make -f build/application.mk`.

## Flashing

The chip's system-memory DFU bootloader is used (nothing on the board is
consumed by it).  With `dfu-util` installed:

1. Hold BOOT0 (or move the BOOT0 jumper to 1), press reset.
2. `make flash_dfu` (= `dfu-util -a 0 -d 0483:df11 -D solo.hex`)
3. Return BOOT0 to 0, reset.  The LED turns solid ON once the firmware is
   running.

SWD (SWDIO=PA13, SWCLK=PA14) works too: `openocd -f
interface/stlink.cfg -f target/stm32f4x.cfg -c "program solo.hex verify
reset exit"`.

## First boot / testing

On first boot with blank flash the device provisions the public **Solo
Hacker** attestation key and certificate into the data segment (the key is
published upstream and documented as not confidential; it derives exactly
the public key embedded in the hacker cert, verified at port time).  The
device then presents the same attestation identity as every factory Solo
Hacker — the honest identity for an unlocked DIY build.  Note the L4
original expects this key to be factory-provisioned; on a bare F411 the
firmware writes it itself.

Test with the solo1 python client (`pip install solo1`):

```
solo1 key poke          # LED blinks, prints button state
solo1 key version       # 4.1.5-...
```

or register it against https://webauthn.io, or with libfido2's
`fido2-cred`/`fido2-token -L`.  Button = the on-board KEY; hold it when
the LED starts blinking during registration/authentication.

## Limitations (hacker-grade)

* Software RNG, no continuous health testing hardware.
* Attestation is a self-generated "Solo Hacker" identity, not
  SoloKeys-rooted; Relying Parties that check attestation trust chains
  will see a hacker key.
* 1-2 s USB-deaf window on data-page erase; small power-loss window (see
  above).
* No NFC, no CCID, no bootloader signature chain, no secure boot; RDP is
  left at level 0 (checked and warned about in `device_migrate`).
* Keep the physical device safe: with no secure boot, an attacker with
  physical access can replace the firmware and extract resident key
  material.  If you want readout protection, set RDP level 1 via SWD —
  note that makes DFU flashing harder (level 1 allows re-flash through
  debug only after mass erase).
