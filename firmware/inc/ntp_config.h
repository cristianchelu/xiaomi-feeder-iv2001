/*
 * NTP server setting — spec/30-processes/time-sync.md § Server
 */

#ifndef NTP_CONFIG_H
#define NTP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

#define NTP_CONFIG_DEFAULT  "pool.ntp.org"
#define NTP_CONFIG_MAX_LEN  63u

/* Configured server (hostname or IPv4), default when unset. */
void ntp_config_get(char *buf, size_t len);
/* Validate (letters, digits, '.', '-') and store; "" restores the default. */
port_err_t ntp_config_set(const char *server);

#endif /* NTP_CONFIG_H */
