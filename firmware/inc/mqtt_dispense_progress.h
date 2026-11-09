/*
 * Live dispense progress — spec/30-processes/mqtt-protocol.md § Dispense progress
 */

#ifndef MQTT_DISPENSE_PROGRESS_H
#define MQTT_DISPENSE_PROGRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "weight_units.h"

/* {"state":"idle"|"running"|"weighing","portions":n,"of":N,"g":"x.y"} retained. */
void mqtt_dispense_progress_set_device_id(const char *device_id);
void mqtt_dispense_progress_update(const char *state, uint8_t done, uint8_t total,
                                   weight_dg_t est_dg);
void mqtt_dispense_progress_connect_snapshot(void);
bool mqtt_dispense_progress_format(char *buf, size_t len);

void mqtt_dispense_progress_test_reset(void);

#endif /* MQTT_DISPENSE_PROGRESS_H */
