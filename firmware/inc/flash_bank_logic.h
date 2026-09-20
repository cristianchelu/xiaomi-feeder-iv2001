/*
 * Pure flash bank helpers — host-tested.
 */

#ifndef FLASH_BANK_LOGIC_H
#define FLASH_BANK_LOGIC_H

#include <stdint.h>

#include "boot_bank.h"
#include "memory_map.h"

boot_bank_t flash_bank_inactive(boot_bank_t active);

uint32_t flash_bank_rom_offset(boot_bank_t bank);

/*
 * Largest erase unit (0x1000, 0x8000 or 0x10000) that starts at
 * ROM_BASE + bank_rom_offset + frontier aligned to its own size and ends at
 * or before the bank end — spec/40-architecture/partition-layout.md
 * § Bank A / Bank B.
 */
uint32_t flash_bank_erase_unit(uint32_t bank_rom_offset, uint32_t frontier);

#endif /* FLASH_BANK_LOGIC_H */
