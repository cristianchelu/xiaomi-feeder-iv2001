/*
 * Dispense amount for the HA Dispense button — spec/30-processes/mqtt-protocol.md
 *
 * Replaces the old "Dispense custom amount" number, which dispensed as soon as
 * its value changed. The amount is now a setting; the Dispense button (and an
 * empty cmd/dispense) uses it.
 */

#include "mqtt_dispense_amount.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_keys.h"
#include "config_port.h"
#include "mqtt_json.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"

#define DISPENSE_AMOUNT_KEY "button_g"

static char s_topic[96];

bool dispense_amount_get(uint16_t *grams)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];
    long v;

    if (grams == NULL || cfg == NULL ||
        cfg->read(CONFIG_GROUP_FEED, DISPENSE_AMOUNT_KEY, buf, sizeof(buf)) != PORT_OK) {
        return false;
    }
    v = strtol(buf, NULL, 10);
    if (v < (long)DISPENSE_AMOUNT_MIN_G || v > (long)DISPENSE_AMOUNT_MAX_G) {
        return false;
    }
    *grams = (uint16_t)v;
    return true;
}

void mqtt_dispense_amount_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "feed/dispense_amount") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

static void mqtt_dispense_amount_publish(void)
{
    char payload[24];
    uint16_t g = 10u;

    if (s_topic[0] == '\0') {
        return;
    }
    (void)dispense_amount_get(&g);
    (void)snprintf(payload, sizeof(payload), "{\"g\":%u}", (unsigned)g);
    (void)mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 1, true);
}

void mqtt_dispense_amount_connect_snapshot(void)
{
    mqtt_dispense_amount_publish();
}

port_err_t mqtt_dispense_amount_handle(const void *payload, size_t len)
{
    const config_port_t *cfg = config_port_get();
    unsigned g = 0u;
    char buf[8];

    if (payload == NULL || len == 0u || cfg == NULL ||
        !mqtt_json_find_uint(payload, len, "g", &g) ||
        g < DISPENSE_AMOUNT_MIN_G || g > DISPENSE_AMOUNT_MAX_G) {
        return PORT_ERR_INVALID_ARG;
    }
    (void)snprintf(buf, sizeof(buf), "%u", g);
    if (cfg->write(CONFIG_GROUP_FEED, DISPENSE_AMOUNT_KEY, buf) != PORT_OK) {
        return PORT_ERR_IO;
    }
    mqtt_dispense_amount_publish();
    return PORT_OK;
}

void mqtt_dispense_amount_test_reset(void)
{
    s_topic[0] = '\0';
}
