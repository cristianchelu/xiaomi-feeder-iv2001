/*
 * Portion size over MQTT — spec/30-processes/mqtt-protocol.md § Portion size
 */

#ifndef MQTT_PORTION_H
#define MQTT_PORTION_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

/* {"g":"8.5","status":"ok"|"failed"|"default"} */
bool mqtt_portion_format_snapshot(char *buf, size_t len);
void mqtt_portion_set_device_id(const char *device_id);
void mqtt_portion_connect_snapshot(void);
void mqtt_portion_sync(void);
/* cmd/portion_cal: {} or {"action":"calibrate"} starts a run; {"action":"reset"} → 10 g. */
port_err_t mqtt_portion_handle(const void *payload, size_t len);

void mqtt_portion_test_reset(void);

#endif /* MQTT_PORTION_H */
