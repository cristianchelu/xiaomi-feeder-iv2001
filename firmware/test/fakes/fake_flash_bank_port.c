#include <string.h>

#include "fake_flash_bank_port.h"

static fake_flash_bank_port_state_t s_state;

void fake_flash_bank_port_reset(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.active_bank = BOOT_BANK_A;
    s_state.hash_result = PORT_OK;
}

void fake_flash_bank_port_set_active(boot_bank_t bank)
{
    s_state.active_bank = bank;
}

void fake_flash_bank_port_set_bank_hash(const uint8_t hash[FLASH_BANK_SHA512_LEN])
{
    memcpy(s_state.bank_hash, hash, FLASH_BANK_SHA512_LEN);
}

void fake_flash_bank_port_set_hash_result(port_err_t result)
{
    s_state.hash_result = result;
}

const fake_flash_bank_port_state_t *fake_flash_bank_port_state(void)
{
    return &s_state;
}

static boot_bank_t fake_get_active_bank(void)
{
    return s_state.active_bank;
}

static port_err_t fake_erase_inactive(void)
{
    s_state.erase_calls++;
    return PORT_OK;
}

static port_err_t fake_write_inactive(uint32_t offset, const uint8_t *data, size_t len)
{
    (void)offset;
    (void)data;
    (void)len;
    s_state.write_calls++;
    return PORT_OK;
}

static port_err_t fake_hash_inactive(uint32_t image_len,
                                     uint8_t hash_out[FLASH_BANK_SHA512_LEN])
{
    s_state.hash_calls++;
    s_state.last_hash_len = image_len;

    if (s_state.hash_result != PORT_OK) {
        return s_state.hash_result;
    }

    memcpy(hash_out, s_state.bank_hash, FLASH_BANK_SHA512_LEN);
    return PORT_OK;
}

static port_err_t fake_swap_banks(const uint8_t image_hash[FLASH_BANK_SHA512_LEN])
{
    s_state.swap_calls++;
    memcpy(s_state.last_swap_hash, image_hash, FLASH_BANK_SHA512_LEN);
    return PORT_OK;
}

static const flash_bank_port_t s_port = {
    .get_active_bank = fake_get_active_bank,
    .erase_inactive = fake_erase_inactive,
    .write_inactive = fake_write_inactive,
    .hash_inactive = fake_hash_inactive,
    .swap_banks = fake_swap_banks,
};

const flash_bank_port_t *fake_flash_bank_port_get(void)
{
    return &s_port;
}
