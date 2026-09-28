/* Copyright 2019 SoloKeys Developers */
/* */
/* Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or */
/* http://www.apache.org/licenses/LICENSE-2.0> or the MIT license */
/* <LICENSE-MIT>, at your option. This file may not be copied, modified, or */
/* distributed except according to those terms. */

/*
STM32F411CEU6 (black pill) solo1 application layout.

  region |  sectors 0..5     | sector 6          | sector 7
  pos    | 0x08000000        | 0x08040000        | 0x08060000
  len    | 256 KB            | 128 KB            | 128 KB (unused)
  desc   | application       | FIDO2 data (15 logical 2KB pages, see
         |                   |  memory_layout.h; staging-dance erase)

No bootloader on this port: the application starts at 0x08000000 and may
occupy all of sectors 0..5.  RAM is one contiguous 128 KB bank.
 */

ENTRY(Reset_Handler)

/* End of RAM */
_estack = 0x20020000;

_MIN_STACK_SIZE = 0x800;

/* Current firmware version number is concatenated to the firmware code - see .flag marker */

MEMORY
{
    flash (rx)      : ORIGIN = 0x08000000, LENGTH = 256K
    ram (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{

    /DISCARD/ :
    {
        /* Unused exception info */
        *(.ARM.exidx);
        *(.ARM.exidx.*);
        *(.ARM.extab.*);
    }

    .isr_vector :
    {
        . = ALIGN(8);
        KEEP(*(.isr_vector))
        . = ALIGN(8);
    } >flash

    .text :
    {
        . = ALIGN(8);
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.finit))
        . = ALIGN(8);
        _etext = .;
    } >flash

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(8);
        _sdata = .;
        *(.data*)
        . = ALIGN(8);
        _edata = .;
    } > ram AT> flash

    .flag :
    {
        . = ALIGN(8);
      KEEP(*(.flag)) ;
    } > flash

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > ram

    ._stack :
    {
        . = ALIGN(8);
        end = .;
        _end = .;
        . = . + _MIN_STACK_SIZE;
        . = ALIGN(8);
    } > ram

}
