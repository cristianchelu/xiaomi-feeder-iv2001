/*
 * Pure flash bank helpers — host-tested.
 */

#include <stddef.h>

#include "flash_bank_logic.h"

boot_bank_t flash_bank_inactive(boot_bank_t active)
{
    return (active == BOOT_BANK_A) ? BOOT_BANK_B : BOOT_BANK_A;
}

uint32_t flash_bank_rom_offset(boot_bank_t bank)
{
    return (bank == BOOT_BANK_B) ? (BANK_B_BASE - ROM_BASE) : (CM4_BASE - ROM_BASE);
}

uint32_t flash_bank_erase_unit(uint32_t bank_rom_offset, uint32_t frontier)
{
    static const uint32_t units[] = { 0x10000u, 0x8000u, 0x1000u };
    uint32_t addr = ROM_BASE + bank_rom_offset + frontier;
    size_t i;

    for (i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        uint32_t unit = units[i];

        if ((addr & (unit - 1u)) == 0u && frontier + unit <= CM4_LENGTH) {
            return unit;
        }
    }

    return 0x1000u;
}
