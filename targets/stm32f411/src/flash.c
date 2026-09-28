// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
//
// STM32F411 flash driver for solo1.
//
// The F411 has non-uniform flash sectors (4x16K, 1x64K, 3x128K) while solo1's
// storage model assumes uniform 2KB pages.  All 15 live data pages
// (ATTESTATION, RK x10, COUNTER x2, STATE x2) are mapped into the tail of
// flash sector 6 (128KB @ 0x08040000); see memory_layout.h.  "Erasing a page"
// therefore means: stage every other live page in RAM, erase the whole
// sector, write the staged pages back.  Sector erase (~1s typ) plus the
// 30KB writeback run with IRQs off, so USB is deaf for roughly 1-2 seconds
// whenever a page erase happens (registration, resident-key reset, counter
// rollover every 256 assertions).  Hosts tolerate this as a slow response;
// CTAPHID keepalives cannot be sent meanwhile because all code lives in the
// (now busy) flash.
//
// Power-loss window: the staging dance is not atomic.  If power is cut in
// the middle, all data pages may come back erased (device re-initializes,
// resident keys lost).  Writeback order below puts the fault-tolerant
// counter pages first to shrink the worst window.  Do not yank the key
// while the LED is doing something unusual.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"

#include APP_CONFIG
#include "flash.h"
#include "log.h"
#include "device.h"
#include "memory_layout.h"

// F4 flash control register bits
#define FLASH_CR_LOCK_Pos       (31U)
#define FLASH_CR_ERRIE_Pos      (25U)
#define FLASH_CR_PSIZE_Pos      (8U)
#define FLASH_CR_PSIZE_WORD     (2U << FLASH_CR_PSIZE_Pos) // 32-bit programming
#define FLASH_CR_SNB_Pos        (3U)
#define FLASH_CR_SNB_Msk        (0xFU << FLASH_CR_SNB_Pos)
#define FLASH_CR_PG             (1U << 0)
#define FLASH_CR_SER            (1U << 1)
#define FLASH_CR_STRT           (1U << 16)
#define FLASH_SR_EOP            (1U << 0)
#define FLASH_SR_ERRORS         ((1U<<4)|(1U<<5)|(1U<<6)|(1U<<7)|(1U<<8)) // WRP|PGA|PGP|PGS|RD
#define FLASH_SR_BSY            (1U << 16)

static void flash_lock(void)
{
    FLASH->CR |= (1U << FLASH_CR_LOCK_Pos);
}

static void flash_unlock(void)
{
    if (FLASH->CR & (1U << FLASH_CR_LOCK_Pos))
    {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
}

// No option bytes to manage on this port: nBOOT0 follows the BOOT0 pin and
// we deliberately never touch OPTCR (a wrong write can lock the chip).
// "boot into DFU" is done by holding BOOT0 high at reset instead.
void flash_option_bytes_init(int boot_from_dfu)
{
    (void) boot_from_dfu;
}

static void flash_wait_idle(void)
{
    while (FLASH->SR & FLASH_SR_BSY)
        ;
}

// Erase one whole F4 flash sector (0..7 on F411xE).
static void flash_erase_sector(uint32_t sector)
{
    flash_wait_idle();
    flash_unlock();

    // clear previous errors (write 1 to clear)
    FLASH->SR = FLASH_SR_ERRORS;

    FLASH->CR &= ~(FLASH_CR_SNB_Msk | FLASH_CR_PG | FLASH_CR_SER);
    FLASH->CR |= (sector << FLASH_CR_SNB_Pos) | FLASH_CR_SER;

    // Go!
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY)
        ;

    if (FLASH->SR & FLASH_SR_ERRORS)
    {
        printf2(TAG_ERR, "erase NOT successful %lx\r\n", FLASH->SR);
        FLASH->SR = FLASH_SR_ERRORS;
    }

    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_PG);
}

// Program one 32-bit word.  Must be word-aligned; target must be erased
// (0xffffffff) - callers never reprogram a live word.
static void flash_program_word(uint32_t addr, uint32_t data)
{
    flash_wait_idle();

    FLASH->SR = FLASH_SR_ERRORS;

    FLASH->CR &= ~FLASH_CR_SER;
    FLASH->CR |= FLASH_CR_PSIZE_WORD | FLASH_CR_PG;

    *(volatile uint32_t *)addr = data;

    while (FLASH->SR & FLASH_SR_BSY)
        ;

    if (FLASH->SR & FLASH_SR_ERRORS)
    {
        printf2(TAG_ERR, "program NOT successful %lx\r\n", FLASH->SR);
        FLASH->SR = FLASH_SR_ERRORS;
    }

    FLASH->SR = FLASH_SR_EOP;
    FLASH->CR &= ~FLASH_CR_PG;
}

// Raw page program: addr must be page aligned, len a multiple of 4.
static void flash_program_range(uint32_t addr, const uint8_t * data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i += 4)
    {
        uint32_t word;
        memmove(&word, data + i, 4);
        flash_program_word(addr + i, word);
    }
}

#define DATA_LIVE_PAGES     15  // ATTESTATION + RK*10 + COUNTER*2 + STATE*2

// Staging buffer: holds every live data page except the one being erased.
static uint8_t staging[DATA_LIVE_PAGES][PAGE_SIZE];

// Order in which staged pages are written back after a sector erase.
// Counter pages first (they are the only ones with a recovery path), then
// state, attestation, resident keys.
static const uint8_t writeback_order[DATA_LIVE_PAGES] = {
    COUNTER1_PAGE, COUNTER2_PAGE,
    STATE1_PAGE, STATE2_PAGE,
    ATTESTATION_PAGE,
    RK_START_PAGE + 0, RK_START_PAGE + 1, RK_START_PAGE + 2, RK_START_PAGE + 3,
    RK_START_PAGE + 4, RK_START_PAGE + 5, RK_START_PAGE + 6, RK_START_PAGE + 7,
    RK_START_PAGE + 8, RK_START_PAGE + 9,
};

void flash_erase_page(uint8_t page)
{
    int i;

    // Only data pages in the managed sector may be erased.
    if (page < PAGES - DATA_LIVE_PAGES || page >= PAGES)
    {
        printf2(TAG_ERR, "flash_erase_page: refusing to erase page %d\r\n", page);
        return;
    }

    __disable_irq();

    // Stage all live pages except the victim.
    for (i = 0; i < DATA_LIVE_PAGES; i++)
    {
        uint8_t p = writeback_order[i];
        if (p == page)
            continue;
        memmove(staging[i], (uint8_t *)flash_addr(p), PAGE_SIZE);
    }

    flash_unlock();
    flash_erase_sector(DATA_FLASH_SECTOR);

    for (i = 0; i < DATA_LIVE_PAGES; i++)
    {
        uint8_t p = writeback_order[i];
        if (p == page)
            continue;
        flash_program_range(flash_addr(p), staging[i], PAGE_SIZE);
    }

    flash_lock();
    __enable_irq();
}

void flash_write_dword(uint32_t addr, uint64_t data)
{
    __disable_irq();
    flash_unlock();
    flash_program_word(addr, (uint32_t)data);
    flash_program_word(addr + 4, (uint32_t)(data >> 32));
    flash_lock();
    __enable_irq();
}

void flash_write(uint32_t addr, uint8_t * data, size_t sz)
{
    unsigned int i;
    uint8_t buf[8];

    // dword align, same semantics as the L4 driver
    addr &= ~(0x07);

    __disable_irq();
    flash_unlock();

    for (i = 0; i < sz; i += 8)
    {
        uint64_t dword;
        memmove(buf, data + i, (sz - i) > 8 ? 8 : sz - i);
        if (sz - i < 8)
        {
            memset(buf + sz - i, 0xff, 8 - (sz - i));
        }
        memmove(&dword, buf, 8);
        flash_program_word(addr + i, (uint32_t)dword);
        flash_program_word(addr + i + 4, (uint32_t)(dword >> 32));
    }

    flash_lock();
    __enable_irq();
}

// Unused on F4 (was an L4 fast-programming path).
void flash_write_fast(uint32_t addr, uint32_t * data)
{
    (void) addr;
    (void) data;
}
