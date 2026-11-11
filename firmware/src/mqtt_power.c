/*
 * Power settings over MQTT — spec/30-processes/mqtt-protocol.md § Power settings
 */

#include "mqtt_power.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "config_keys.h"
#include "config_port.h"
#include "mqtt_json.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"

static char s_topic[96];
static char s_last[48];
static bool s_last_valid;

battery_wifi_t mqtt_power_battery_wifi_get(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[20];
    battery_wifi_t mode;

    if (cfg == NULL ||
        cfg->read(CONFIG_GROUP_POWER, CONFIG_KEY_POWER_BATTERY_WIFI, buf, sizeof(buf)) != PORT_OK ||
        !power_policy_battery_wifi_parse(buf, &mode)) {
        return BATTERY_WIFI_ON;
    }
    return mode;
}

bool mqtt_power_format_snapshot(char *buf, size_t len)
{
    int w;

    if (buf == NULL) {
        return false;
    }
    w = snprintf(buf, len, "{\"battery_wifi\":\"%s\"}",
                 power_policy_battery_wifi_name(mqtt_power_battery_wifi_get()));
    return w > 0 && (size_t)w < len;
}

void mqtt_power_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "power") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

static void mqtt_power_publish(void)
{
    char payload[sizeof(s_last)];

    if (s_topic[0] == '\0' || !mqtt_power_format_snapshot(payload, sizeof(payload))) {
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

void mqtt_power_connect_snapshot(void)
{
    s_last_valid = false;
    mqtt_power_publish();
}

port_err_t mqtt_power_handle(const void *payload, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char name[20];
    battery_wifi_t mode;

    if (payload == NULL || len == 0u || cfg == NULL ||
        !mqtt_json_find_string(payload, len, "battery_wifi", name, sizeof(name)) ||
        !power_policy_battery_wifi_parse(name, &mode)) {
        return PORT_ERR_INVALID_ARG;
    }
    if (mqtt_power_battery_wifi_get() != mode) {
        if (cfg->write(CONFIG_GROUP_POWER, CONFIG_KEY_POWER_BATTERY_WIFI,
                       power_policy_battery_wifi_name(mode)) != PORT_OK) {
            return PORT_ERR_IO;
        }
        app_log_info("app", "battery wifi: %s", power_policy_battery_wifi_name(mode));
    }
    mqtt_power_publish();
    return PORT_OK;
}

void mqtt_power_test_reset(void)
{
    s_topic[0] = '\0';
    s_last[0] = '\0';
    s_last_valid = false;
}
