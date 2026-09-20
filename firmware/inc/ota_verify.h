/*
 * OTA image verification — spec/30-processes/ota-flow.md § Verification
 */

#ifndef OTA_VERIFY_H
#define OTA_VERIFY_H

#include <stdint.h>

#include "flash_bank_port.h"
#include "port_err.h"

/*
 * Hash the first image_len bytes of the inactive bank into bank_hash_out and,
 * when expected_sha512 is non-NULL, require it to match.
 *
 * Returns PORT_OK on success, PORT_ERR_INVALID_ARG on a manifest mismatch or
 * bad arguments, and the flash port's error when the bank cannot be hashed.
 */
port_err_t ota_verify_bank(const flash_bank_port_t *flash,
                           uint32_t image_len,
                           const uint8_t *expected_sha512,
                           uint8_t bank_hash_out[FLASH_BANK_SHA512_LEN]);

#endif /* OTA_VERIFY_H */
