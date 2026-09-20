/* Tests: spec/30-processes/ota-flow.md, spec/40-architecture/partition-layout.md */

#include "unity.h"

#include "flash_bank_logic.h"
#include "boot_bank.h"

void test_inactive_bank_from_a(void)
{
    TEST_ASSERT_EQUAL(BOOT_BANK_B, flash_bank_inactive(BOOT_BANK_A));
}

void test_inactive_bank_from_b(void)
{
    TEST_ASSERT_EQUAL(BOOT_BANK_A, flash_bank_inactive(BOOT_BANK_B));
}

void test_bank_a_rom_offset(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x00012000, flash_bank_rom_offset(BOOT_BANK_A));
}

void test_bank_b_rom_offset(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x00100000, flash_bank_rom_offset(BOOT_BANK_B));
}

/* spec/40-architecture/partition-layout.md § Bank A / Bank B — erase units */

void test_bank_a_erase_starts_with_sectors_until_32k_boundary(void)
{
    uint32_t base = flash_bank_rom_offset(BOOT_BANK_A);

    TEST_ASSERT_EQUAL_HEX32(0x1000, flash_bank_erase_unit(base, 0x0000));
    TEST_ASSERT_EQUAL_HEX32(0x1000, flash_bank_erase_unit(base, 0x5000));
    TEST_ASSERT_EQUAL_HEX32(0x8000, flash_bank_erase_unit(base, 0x6000)); /* 0x08018000 */
    TEST_ASSERT_EQUAL_HEX32(0x10000, flash_bank_erase_unit(base, 0xE000)); /* 0x08020000 */
}

void test_bank_b_erase_ends_with_32k_and_sectors_before_nvdm(void)
{
    uint32_t base = flash_bank_rom_offset(BOOT_BANK_B);

    TEST_ASSERT_EQUAL_HEX32(0x10000, flash_bank_erase_unit(base, 0x00000));
    TEST_ASSERT_EQUAL_HEX32(0x10000, flash_bank_erase_unit(base, 0xD0000));
    TEST_ASSERT_EQUAL_HEX32(0x8000, flash_bank_erase_unit(base, 0xE0000)); /* 64K would cross NVDM */
    TEST_ASSERT_EQUAL_HEX32(0x1000, flash_bank_erase_unit(base, 0xE8000));
    TEST_ASSERT_EQUAL_HEX32(0x1000, flash_bank_erase_unit(base, 0xED000));
}

static void walk_bank(boot_bank_t bank, unsigned *n4k, unsigned *n32k, unsigned *n64k)
{
    uint32_t base = flash_bank_rom_offset(bank);
    uint32_t frontier = 0;

    *n4k = *n32k = *n64k = 0;
    while (frontier < CM4_LENGTH) {
        uint32_t unit = flash_bank_erase_unit(base, frontier);
        uint32_t addr = ROM_BASE + base + frontier;

        TEST_ASSERT_EQUAL_HEX32(0, addr & (unit - 1u));      /* aligned to its size */
        TEST_ASSERT_TRUE(frontier + unit <= CM4_LENGTH);     /* inside the bank */
        if (unit == 0x1000) (*n4k)++;
        else if (unit == 0x8000) (*n32k)++;
        else if (unit == 0x10000) (*n64k)++;
        else TEST_FAIL_MESSAGE("unknown erase unit");
        frontier += unit;
    }
    TEST_ASSERT_EQUAL_HEX32(CM4_LENGTH, frontier);           /* lands exactly on the bank end */
}

void test_erase_plan_covers_bank_a_exactly(void)
{
    unsigned n4k, n32k, n64k;

    walk_bank(BOOT_BANK_A, &n4k, &n32k, &n64k);
    TEST_ASSERT_EQUAL_UINT(6, n4k);
    TEST_ASSERT_EQUAL_UINT(1, n32k);
    TEST_ASSERT_EQUAL_UINT(14, n64k);
}

void test_erase_plan_covers_bank_b_exactly(void)
{
    unsigned n4k, n32k, n64k;

    walk_bank(BOOT_BANK_B, &n4k, &n32k, &n64k);
    TEST_ASSERT_EQUAL_UINT(14, n64k);
    TEST_ASSERT_EQUAL_UINT(1, n32k);
    TEST_ASSERT_EQUAL_UINT(6, n4k);
}
