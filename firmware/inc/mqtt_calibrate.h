/*
 * MQTT scale calibration command — spec/30-processes/mqtt-protocol.md § Calibrate command
 */

#ifndef MQTT_CALIBRATE_H
#define MQTT_CALIBRATE_H

#include <stddef.h>

#include "port_err.h"

port_err_t mqtt_calibrate_handle(const void *payload, size_t len);

#endif /* MQTT_CALIBRATE_H */
