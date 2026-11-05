/*
 * Child lock MQTT publisher and command handler —
 * spec/30-processes/mqtt-protocol.md § Child lock
 */

#ifndef MQTT_CHILD_LOCK_H
#define MQTT_CHILD_LOCK_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

void mqtt_child_lock_set_device_id(const char *device_id);
void mqtt_child_lock_publish_snapshot(void);
void mqtt_child_lock_connect_snapshot(void);
port_err_t mqtt_child_lock_apply(bool locked);
port_err_t mqtt_child_lock_handle(const void *payload, size_t len);

void mqtt_child_lock_test_reset(void);

#endif /* MQTT_CHILD_LOCK_H */
