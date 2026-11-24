/*
 * Deferred device reboot — spec/30-processes/mqtt-protocol.md § Reboot command
 */

#ifndef REBOOT_REQUEST_H
#define REBOOT_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include "port_err.h"

#define REBOOT_REQUEST_DELAY_MS 500u /* [tune] */

/* PORT_ERR_BUSY while a dispense or OTA is active. */
port_err_t reboot_request_arm(void);
bool reboot_request_pending(void);
/* Call from the display tick; reboots once the deadline passes. */
void reboot_request_poll(uint32_t now_ms);
void reboot_request_test_reset(void);

#endif /* REBOOT_REQUEST_H */
