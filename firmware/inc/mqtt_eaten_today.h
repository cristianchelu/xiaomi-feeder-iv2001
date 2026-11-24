/*
 * Eaten-today MQTT publisher — spec/30-processes/mqtt-protocol.md § Eaten today
 */

#ifndef MQTT_EATEN_TODAY_H
#define MQTT_EATEN_TODAY_H

#include <stdbool.h>
#include <stddef.h>

#include "weight_units.h"

bool mqtt_eaten_today_format(weight_dg_t dg, char *buf, size_t len);
void mqtt_eaten_today_set_device_id(const char *device_id);
void mqtt_eaten_today_sync(void);
void mqtt_eaten_today_connect_snapshot(void);

void mqtt_eaten_today_test_reset(void);

#endif /* MQTT_EATEN_TODAY_H */
