/*
 * Dispense amount for the HA Dispense button — spec/30-processes/mqtt-protocol.md
 */

#ifndef MQTT_DISPENSE_AMOUNT_H
#define MQTT_DISPENSE_AMOUNT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port_err.h"

#define DISPENSE_AMOUNT_MIN_G  5u
#define DISPENSE_AMOUNT_MAX_G  150u

/* Stored amount; false when never set (button keeps its 1-portion default). */
bool dispense_amount_get(uint16_t *grams);
void mqtt_dispense_amount_set_device_id(const char *device_id);
void mqtt_dispense_amount_connect_snapshot(void);
/* cmd/dispense_amount {"g":N} — stores N, does not dispense. */
port_err_t mqtt_dispense_amount_handle(const void *payload, size_t len);

void mqtt_dispense_amount_test_reset(void);

#endif /* MQTT_DISPENSE_AMOUNT_H */
