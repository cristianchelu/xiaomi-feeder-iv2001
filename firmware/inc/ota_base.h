/*
 * OTA firmware base URL — spec/30-processes/ota-flow.md § One-button install
 */

#ifndef OTA_BASE_H
#define OTA_BASE_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

#define OTA_BASE_MAX_LEN  120u

/* Stored base URL (no trailing slash). false when unset. */
bool ota_base_get(char *buf, size_t len);
/* "<base>/petfeeder_<letter>.bin" for the bank to be written ('a' or 'b'). */
bool ota_base_build_url(char bank_letter, char *buf, size_t len);

void ota_base_set_device_id(const char *device_id);
void ota_base_connect_snapshot(void);
/* cmd/ota_base: plain-text http(s) URL. */
port_err_t ota_base_handle(const void *payload, size_t len);

void ota_base_test_reset(void);

#endif /* OTA_BASE_H */
