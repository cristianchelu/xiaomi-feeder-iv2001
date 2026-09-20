/*
 * OTA image verification — spec/30-processes/ota-flow.md § Verification
 *
 * The verified object is the flash contents that will boot; bytes received
 * over HTTP are never hashed directly.
 */

#include <string.h>

#include "ota_verify.h"

port_err_t ota_verify_bank(const flash_bank_port_t *flash,
                           uint32_t image_len,
                           const uint8_t *expected_sha512,
                           uint8_t bank_hash_out[FLASH_BANK_SHA512_LEN])
{
    port_err_t err;

    if (flash == NULL || flash->hash_inactive == NULL || bank_hash_out == NULL) {
        return PORT_ERR_INVALID_ARG;
    }

    err = flash->hash_inactive(image_len, bank_hash_out);
    if (err != PORT_OK) {
        return err;
    }

    if (expected_sha512 != NULL &&
        memcmp(bank_hash_out, expected_sha512, FLASH_BANK_SHA512_LEN) != 0) {
        return PORT_ERR_INVALID_ARG;
    }

    return PORT_OK;
}
