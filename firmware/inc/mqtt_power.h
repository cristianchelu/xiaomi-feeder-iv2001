/*
 * Power settings over MQTT — spec/30-processes/mqtt-protocol.md § Power settings
 */

#ifndef MQTT_POWER_H
#define MQTT_POWER_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"
#include "power_policy.h"

battery_wifi_t mqtt_power_battery_wifi_get(void);
bool mqtt_power_format_snapshot(char *buf, size_t len);
void mqtt_power_set_device_id(const char *device_id);
void mqtt_power_connect_snapshot(void);
/* cmd/power: {"battery_wifi":"on"|"off"|"scheduled_only"} */
port_err_t mqtt_power_handle(const void *payload, size_t len);

void mqtt_power_test_reset(void);

#endif /* MQTT_POWER_H */
