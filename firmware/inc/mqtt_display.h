/*
 * Display settings MQTT publisher and command handler —
 * spec/30-processes/mqtt-protocol.md § Display
 */

#ifndef MQTT_DISPLAY_H
#define MQTT_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

bool mqtt_display_format_snapshot(char *buf, size_t len);
void mqtt_display_set_device_id(const char *device_id);
void mqtt_display_publish_snapshot(void);
void mqtt_display_connect_snapshot(void);
/* Validates every key first; applies to NVDM and publishes only on change. */
port_err_t mqtt_display_handle(const void *payload, size_t len);

void mqtt_display_test_reset(void);

#endif /* MQTT_DISPLAY_H */
