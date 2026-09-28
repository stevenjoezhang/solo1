// STM32F411 port of solo1 target layout.
//
// The F411CE has 512KB flash in *sectors* (4x16K + 1x64K + 3x128K), not
// uniform 2KB pages like the L432.  We keep the logical 2KB page model:
//   - logical pages   0..127  (256KB) -> app region, sectors 0..5
//   - logical pages 128..191  (128KB) -> data region, sector 6 (0x08040000)
//   - sector 7 (0x08060000) unused / reserved
// Because 0x08040000 is exactly logical page 128, the classic
// `0x08000000 + page*2048` mapping works unchanged for every page.
//
// The 15 data pages live at the tail of sector 6.  flash_erase_page()
// stages the other live pages in RAM, erases the whole 128KB sector and
// writes them back (see flash.c).
#ifndef _MEMORY_LAYOUT_H_
#define _MEMORY_LAYOUT_H_

#define PAGE_SIZE		2048
#define PAGES			192

// First logical page of the data sector (sector 6 @ 0x08040000).
#define DATA_SECTOR_BASE_ADDR	0x08040000
#define DATA_START_PAGE		(DATA_SECTOR_BASE_ADDR - 0x08000000) / PAGE_SIZE
// Sector number of the data sector (F4 flash sector erase granularity).
#define DATA_FLASH_SECTOR	6

// Pages 177-191 are data
// Location of counter page and it's backup page
// The flash is wear leveled and counter should be fault tolerant
#define	COUNTER2_PAGE	(PAGES - 4)
#define	COUNTER1_PAGE	(PAGES - 3)

// State of FIDO2 application
#define	STATE2_PAGE		      (PAGES - 2)
#define	STATE1_PAGE		      (PAGES - 1)

#define	STATE1_PAGE_ADDR		(0x08000000 + ((STATE1_PAGE)*PAGE_SIZE))
#define	STATE2_PAGE_ADDR		(0x08000000 + ((STATE2_PAGE)*PAGE_SIZE))

// Storage of FIDO2 resident keys
#define RK_NUM_PAGES    10
#define RK_START_PAGE   (PAGES - 14)
#define RK_END_PAGE     (PAGES - 14 + RK_NUM_PAGES)     // not included

// Start of application code: no bootloader on this target, app starts at 0.
#ifndef APPLICATION_START_PAGE
#define APPLICATION_START_PAGE	(0)
#endif
#define APPLICATION_START_ADDR	(0x08000000 + ((APPLICATION_START_PAGE)*PAGE_SIZE))

// where attestation key is located
#define ATTESTATION_PAGE        (PAGES - 15)
#define ATTESTATION_PAGE_ADDR   (0x08000000 + ATTESTATION_PAGE*PAGE_SIZE)

// End of application code.  Application must stay inside sectors 0..5
// (0x08000000..0x0803FFFF).  Sector 5 ends at logical page 128.
#define APPLICATION_END_PAGE	(128)
#define APPLICATION_END_ADDR	((0x08000000 + ((APPLICATION_END_PAGE)*PAGE_SIZE))-8)

// Bootloader state (kept for source compat; no bootloader on F411 port).
#define AUTH_WORD_ADDR          (APPLICATION_END_ADDR)

#define LAST_ADDR       (APPLICATION_END_ADDR-2048 + 8)
#define BOOT_VERSION_PAGE    (APPLICATION_END_PAGE)
#define BOOT_VERSION_ADDR    (0x08000000 + BOOT_VERSION_PAGE*PAGE_SIZE + 8)
#define LAST_PAGE       (APPLICATION_END_PAGE-1)

#define ATTESTATION_CONFIGURED_TAG      0xaa551e79

struct flash_attestation_page{
  uint8_t attestation_key[32];
  // DWORD padded.
  uint64_t device_settings;
  uint64_t attestation_cert_size;
  uint8_t attestation_cert[2048 - 32 - 8 - 8];
} __attribute__((packed));

typedef struct flash_attestation_page flash_attestation_page;

#include <assert.h>
_Static_assert(sizeof(flash_attestation_page) == 2048, "Data structure doesn't match flash size");
_Static_assert(((DATA_SECTOR_BASE_ADDR - 0x08000000) % PAGE_SIZE) == 0, "data sector must be page aligned");

#endif
