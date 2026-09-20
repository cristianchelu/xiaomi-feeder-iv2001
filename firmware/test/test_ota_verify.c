/* Tests: spec/30-processes/ota-flow.md § Verification */

#include <string.h>

#include "unity.h"

#include "fake_flash_bank_port.h"
#include "ota_verify.h"

#define IMAGE_LEN 525672u

static uint8_t s_bank_hash[FLASH_BANK_SHA512_LEN];
static uint8_t s_other_hash[FLASH_BANK_SHA512_LEN];

static void setup_bank(void)
{
    size_t i;

    fake_flash_bank_port_reset();
    for (i = 0; i < FLASH_BANK_SHA512_LEN; i++) {
        s_bank_hash[i] = (uint8_t)(0xA0u + i);
        s_other_hash[i] = (uint8_t)(0x10u + i);
    }
    fake_flash_bank_port_set_bank_hash(s_bank_hash);
}

void test_ota_verify_hashes_bank_contents_over_image_len(void)
{
    uint8_t out[FLASH_BANK_SHA512_LEN];
    const fake_flash_bank_port_state_t *st;

    setup_bank();
    memset(out, 0, sizeof(out));

    TEST_ASSERT_EQUAL(PORT_OK,
                      ota_verify_bank(fake_flash_bank_port_get(), IMAGE_LEN, NULL, out));

    st = fake_flash_bank_port_state();
    TEST_ASSERT_EQUAL_UINT(1, st->hash_calls);
    TEST_ASSERT_EQUAL_UINT32(IMAGE_LEN, st->last_hash_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(s_bank_hash, out, FLASH_BANK_SHA512_LEN);
}

void test_ota_verify_manifest_match_accepts_bank_hash(void)
{
    uint8_t out[FLASH_BANK_SHA512_LEN];

    setup_bank();

    TEST_ASSERT_EQUAL(PORT_OK,
                      ota_verify_bank(fake_flash_bank_port_get(), IMAGE_LEN, s_bank_hash, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(s_bank_hash, out, FLASH_BANK_SHA512_LEN);
}

void test_ota_verify_manifest_mismatch_rejects(void)
{
    uint8_t out[FLASH_BANK_SHA512_LEN];

    setup_bank();

    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG,
                      ota_verify_bank(fake_flash_bank_port_get(), IMAGE_LEN, s_other_hash, out));
}

void test_ota_verify_bank_read_failure_propagates(void)
{
    uint8_t out[FLASH_BANK_SHA512_LEN];

    setup_bank();
    fake_flash_bank_port_set_hash_result(PORT_ERR_IO);

    TEST_ASSERT_EQUAL(PORT_ERR_IO,
                      ota_verify_bank(fake_flash_bank_port_get(), IMAGE_LEN, s_bank_hash, out));
}

void test_ota_verify_rejects_null_arguments(void)
{
    uint8_t out[FLASH_BANK_SHA512_LEN];

    setup_bank();

    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG,
                      ota_verify_bank(NULL, IMAGE_LEN, NULL, out));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG,
                      ota_verify_bank(fake_flash_bank_port_get(), IMAGE_LEN, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(0, fake_flash_bank_port_state()->hash_calls);
}
