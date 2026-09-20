/*
 * Host fake for flash_bank_port — OTA verification tests.
 */

#ifndef FAKE_FLASH_BANK_PORT_H
#define FAKE_FLASH_BANK_PORT_H

#include <stdint.h>

#include "flash_bank_port.h"

typedef struct fake_flash_bank_port_state {
    boot_bank_t active_bank;
    unsigned erase_calls;
    unsigned write_calls;
    unsigned hash_calls;
    uint32_t last_hash_len;
    port_err_t hash_result;
    uint8_t bank_hash[FLASH_BANK_SHA512_LEN];
    unsigned swap_calls;
    uint8_t last_swap_hash[FLASH_BANK_SHA512_LEN];
} fake_flash_bank_port_state_t;

void fake_flash_bank_port_reset(void);
void fake_flash_bank_port_set_active(boot_bank_t bank);
void fake_flash_bank_port_set_bank_hash(const uint8_t hash[FLASH_BANK_SHA512_LEN]);
void fake_flash_bank_port_set_hash_result(port_err_t result);
const flash_bank_port_t *fake_flash_bank_port_get(void);
const fake_flash_bank_port_state_t *fake_flash_bank_port_state(void);

#endif /* FAKE_FLASH_BANK_PORT_H */
