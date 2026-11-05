/*
 * Display settings MQTT publisher and command handler —
 * spec/30-processes/mqtt-protocol.md § Display
 */

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "display_settings.h"
#include "mqtt_display.h"
#include "mqtt_json.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "port_err.h"

static char s_display_topic[96];
static char s_last_payload[48];
static bool s_last_payload_valid;

bool mqtt_display_format_snapshot(char *buf, size_t len)
{
    int written;

    if (buf == NULL || len == 0u) {
        return false;
    }
    written = snprintf(buf,
                       len,
                       "{\"mode\":\"%s\",\"brightness\":%u}",
                       display_settings_mode_name(display_settings_mode_get()),
                       (unsigned)display_settings_brightness_get());
    return written > 0 && (size_t)written < len;
}

static bool mqtt_display_publish(void)
{
    char payload[sizeof(s_last_payload)];

    if (s_display_topic[0] == '\0' || !mqtt_display_format_snapshot(payload, sizeof(payload))) {
        return false;
    }
    if (s_last_payload_valid && strcmp(s_last_payload, payload) == 0) {
        return true;
    }
    if (!mqtt_outbox_enqueue(s_display_topic, payload, strlen(payload), 1, true)) {
        app_log_debug("mqtt", "display enqueue failed topic=%s", s_display_topic);
        return false;
    }
    (void)snprintf(s_last_payload, sizeof(s_last_payload), "%s", payload);
    s_last_payload_valid = true;
    return true;
}

void mqtt_display_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_display_topic, sizeof(s_display_topic), device_id, "display")
            != PORT_OK) {
        s_display_topic[0] = '\0';
    }
}

void mqtt_display_publish_snapshot(void)
{
    (void)mqtt_display_publish();
}

void mqtt_display_connect_snapshot(void)
{
    s_last_payload_valid = false;
    mqtt_display_publish_snapshot();
}

port_err_t mqtt_display_handle(const void *payload, size_t len)
{
    const char *json = payload;
    bool has_mode;
    bool has_brightness;
    display_mode_t mode = DISPLAY_MODE_WEIGHT;
    unsigned brightness = 0u;
    bool changed = false;

    if (json == NULL || len == 0u) {
        return PORT_ERR_INVALID_ARG;
    }

    has_mode = mqtt_json_has_key(json, len, "mode");
    has_brightness = mqtt_json_has_key(json, len, "brightness");
    if (!has_mode && !has_brightness) {
        return PORT_ERR_INVALID_ARG;
    }

    if (has_mode) {
        char name[16];

        if (!mqtt_json_find_string(json, len, "mode", name, sizeof(name)) ||
            !display_settings_mode_parse(name, &mode)) {
            return PORT_ERR_INVALID_ARG;
        }
    }
    if (has_brightness) {
        if (!mqtt_json_find_uint(json, len, "brightness", &brightness) ||
            brightness < DISPLAY_BRIGHTNESS_MIN || brightness > DISPLAY_BRIGHTNESS_MAX) {
            return PORT_ERR_INVALID_ARG;
        }
    }

    if (has_mode && display_settings_mode_get() != mode) {
        if (!display_settings_mode_set(mode)) {
            return PORT_ERR_IO;
        }
        changed = true;
    }
    if (has_brightness && display_settings_brightness_get() != (uint8_t)brightness) {
        if (!display_settings_brightness_set((uint8_t)brightness)) {
            return PORT_ERR_IO;
        }
        changed = true;
    }

    if (changed) {
        app_log_info("app", "display settings changed");
        mqtt_display_publish_snapshot();
    }
    return PORT_OK;
}

void mqtt_display_test_reset(void)
{
    s_display_topic[0] = '\0';
    s_last_payload[0] = '\0';
    s_last_payload_valid = false;
}
