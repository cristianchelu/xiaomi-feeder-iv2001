/*
 * Live dispense progress — spec/30-processes/mqtt-protocol.md § Dispense progress
 */

#include "mqtt_dispense_progress.h"

#include <stdio.h>
#include <string.h>

#include "mqtt_outbox.h"
#include "mqtt_topics.h"

#define MQTT_DISPENSE_PROGRESS_MAX_PENDING 8u

static char s_topic[96];
static char s_payload[96] = "{\"state\":\"idle\",\"portions\":0,\"of\":0,\"g\":\"0.0\"}";
static char s_sent[96];

void mqtt_dispense_progress_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "dispense/progress") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

static void mqtt_dispense_progress_publish(bool force)
{
    if (s_topic[0] == '\0' || !mqtt_outbox_is_accepting()) {
        return;
    }
    if (!force && strcmp(s_sent, s_payload) == 0) {
        return;
    }
    /* Best effort: leave the outbox to state/event messages when it is busy. */
    if (mqtt_outbox_pending() >= MQTT_DISPENSE_PROGRESS_MAX_PENDING) {
        return;
    }
    if (mqtt_outbox_enqueue(s_topic, s_payload, strlen(s_payload), 0, true)) {
        (void)snprintf(s_sent, sizeof(s_sent), "%s", s_payload);
    }
}

void mqtt_dispense_progress_update(const char *state, uint8_t done, uint8_t total,
                                   weight_dg_t est_dg)
{
    if (state == NULL) {
        return;
    }
    if (est_dg < 0) {
        est_dg = 0;
    }
    (void)snprintf(s_payload, sizeof(s_payload),
                   "{\"state\":\"%s\",\"portions\":%u,\"of\":%u,\"g\":\"%ld.%ld\"}",
                   state, (unsigned)done, (unsigned)total,
                   (long)(est_dg / 10), (long)(est_dg % 10));
    mqtt_dispense_progress_publish(false);
}

void mqtt_dispense_progress_connect_snapshot(void)
{
    s_sent[0] = '\0'; /* if skipped now, the next poll publishes it */
    mqtt_dispense_progress_publish(true);
}

bool mqtt_dispense_progress_format(char *buf, size_t len)
{
    int w;

    if (buf == NULL) {
        return false;
    }
    w = snprintf(buf, len, "%s", s_payload);
    return w > 0 && (size_t)w < len;
}

void mqtt_dispense_progress_test_reset(void)
{
    s_topic[0] = '\0';
    s_sent[0] = '\0';
    (void)snprintf(s_payload, sizeof(s_payload),
                   "{\"state\":\"idle\",\"portions\":0,\"of\":0,\"g\":\"0.0\"}");
}
