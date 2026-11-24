/*
 * Wi-Fi signal strength — spec/30-processes/mqtt-protocol.md § Wi-Fi signal
 */

#include "mqtt_wifi_rssi.h"

#include <stdio.h>
#include <string.h>

#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "wifi_port.h"

static char s_topic[96];
static uint32_t s_last_ms;
static bool s_sampled;
static int8_t s_last_dbm;
static bool s_published;
static uint32_t s_pub_ms;
static int8_t s_pub_dbm;

void mqtt_wifi_rssi_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "wifi_rssi") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

void mqtt_wifi_rssi_poll(uint32_t now_ms, bool force)
{
    const wifi_port_t *wifi = wifi_port_get();
    int8_t dbm;
    char payload[8];

    if (!force && s_sampled && (uint32_t)(now_ms - s_last_ms) < MQTT_WIFI_RSSI_PERIOD_MS) {
        return;
    }
    if (wifi == NULL || wifi->get_rssi == NULL || wifi->is_connected == NULL ||
        !wifi->is_connected() || wifi->get_rssi(&dbm) != PORT_OK) {
        return;
    }
    s_last_ms = now_ms;
    s_sampled = true;
    s_last_dbm = dbm;
    if (s_topic[0] == '\0' || !mqtt_outbox_is_accepting()) {
        return;
    }
    if (!force && s_published &&
        (uint32_t)(now_ms - s_pub_ms) < MQTT_WIFI_RSSI_PUBLISH_MS &&
        dbm - s_pub_dbm < MQTT_WIFI_RSSI_DELTA_DB && s_pub_dbm - dbm < MQTT_WIFI_RSSI_DELTA_DB) {
        return;
    }
    (void)snprintf(payload, sizeof(payload), "%d", (int)dbm);
    if (mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 0, true)) {
        s_published = true;
        s_pub_ms = now_ms;
        s_pub_dbm = dbm;
    }
}

bool mqtt_wifi_rssi_last(int8_t *dbm)
{
    if (dbm == NULL || !s_sampled) {
        return false;
    }
    *dbm = s_last_dbm;
    return true;
}

void mqtt_wifi_rssi_test_reset(void)
{
    s_topic[0] = '\0';
    s_sampled = false;
    s_last_ms = 0u;
    s_published = false;
}
