/*
 * Portion size over MQTT — spec/30-processes/mqtt-protocol.md § Portion size
 */

#include "mqtt_portion.h"

#include <stdio.h>
#include <string.h>

#include "dispense.h"
#include "mqtt_json.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "portion_estimate.h"

static char s_topic[96];
static char s_last[64];
static bool s_last_valid;

static const char *mqtt_portion_status_name(portion_cal_status_t st)
{
    switch (st) {
    case PORTION_CAL_OK:
        return "ok";
    case PORTION_CAL_FAILED:
        return "failed";
    default:
        return "default";
    }
}

bool mqtt_portion_format_snapshot(char *buf, size_t len)
{
    weight_dg_t dg = portion_estimate_dg();
    int w;

    if (buf == NULL) {
        return false;
    }
    w = snprintf(buf, len, "{\"g\":\"%ld.%ld\",\"status\":\"%s\"}",
                 (long)(dg / 10), (long)(dg % 10),
                 mqtt_portion_status_name(portion_estimate_status()));
    return w > 0 && (size_t)w < len;
}

void mqtt_portion_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "portion") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

void mqtt_portion_sync(void)
{
    char payload[sizeof(s_last)];

    if (s_topic[0] == '\0' || !mqtt_portion_format_snapshot(payload, sizeof(payload))) {
        return;
    }
    if (s_last_valid && strcmp(payload, s_last) == 0) {
        return;
    }
    if (!mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 1, true)) {
        return;
    }
    (void)snprintf(s_last, sizeof(s_last), "%s", payload);
    s_last_valid = true;
}

void mqtt_portion_connect_snapshot(void)
{
    s_last_valid = false;
    mqtt_portion_sync();
}

port_err_t mqtt_portion_handle(const void *payload, size_t len)
{
    char action[16] = "calibrate";

    if (payload != NULL && len > 0u) {
        (void)mqtt_json_find_string(payload, len, "action", action, sizeof(action));
    }
    if (strcmp(action, "reset") == 0) {
        portion_estimate_reset();
        mqtt_portion_sync();
        return PORT_OK;
    }
    if (strcmp(action, "calibrate") != 0) {
        return PORT_ERR_INVALID_ARG;
    }
    switch (dispense_submit_portion_cal(DISPENSE_SOURCE_MQTT)) {
    case DISPENSE_SUBMIT_OK:
        return PORT_OK;
    case DISPENSE_SUBMIT_BUSY:
    case DISPENSE_SUBMIT_QUEUE_FULL:
        return PORT_ERR_BUSY;
    default:
        return PORT_ERR_INVALID_ARG;
    }
}

void mqtt_portion_test_reset(void)
{
    s_topic[0] = '\0';
    s_last[0] = '\0';
    s_last_valid = false;
}
