/*
 * Problem state over MQTT — spec/30-processes/health-monitoring.md § Problem state
 */

#ifndef MQTT_PROBLEM_H
#define MQTT_PROBLEM_H

#include <stddef.h>

#include "port_err.h"

void mqtt_problem_set_device_id(const char *device_id);
/* {"active":…,"codes":[…],"text":"…"[,"last_feed":{…}]} */
int mqtt_problem_format(char *buf, size_t len);
/* Publish when the payload changed (app display tick). */
void mqtt_problem_sync(void);
void mqtt_problem_connect_snapshot(void);
port_err_t mqtt_problem_handle_clear(void);

void mqtt_problem_test_reset(void);

#endif /* MQTT_PROBLEM_H */
