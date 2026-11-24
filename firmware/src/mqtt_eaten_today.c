/*
 * Eaten-today MQTT publisher — spec/30-processes/mqtt-protocol.md § Eaten today
 */

#include "mqtt_eaten_today.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "eaten_today.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "port_err.h"

static char s_topic[96];
static char s_last[16];
static bool s_last_valid;

bool mqtt_eaten_today_format(weight_dg_t dg, char *buf, size_t len)
{
    int written;

    if (buf == NULL || len == 0u) {
        return false;
    }
    if (dg < 0) {
        dg = 0;
    }
    written = snprintf(buf, len, "%ld.%ld", (long)(dg / 10), (long)(dg % 10));
    return written > 0 && (size_t)written < len;
}

void mqtt_eaten_today_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "eaten_today") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

void mqtt_eaten_today_sync(void)
{
    char payload[sizeof(s_last)];

    if (s_topic[0] == '\0' ||
        !mqtt_eaten_today_format(eaten_today_total_dg(), payload, sizeof(payload))) {
        return;
    }
    if (s_last_valid && strcmp(payload, s_last) == 0) {
        return;
    }
    if (!mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 1, true)) {
        app_log_debug("mqtt", "eaten_today enqueue failed");
        return;
    }
    (void)snprintf(s_last, sizeof(s_last), "%s", payload);
    s_last_valid = true;
}

void mqtt_eaten_today_connect_snapshot(void)
{
    s_last_valid = false;
    mqtt_eaten_today_sync();
}

void mqtt_eaten_today_test_reset(void)
{
    s_topic[0] = '\0';
    s_last[0] = '\0';
    s_last_valid = false;
}
