/*
 * Child lock MQTT publisher and command handler —
 * spec/30-processes/mqtt-protocol.md § Child lock
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "feed_config.h"
#include "mqtt_child_lock.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "port_err.h"

static char s_child_lock_topic[96];
static char s_last_payload[4];
static bool s_last_payload_valid;

static const char *mqtt_child_lock_payload(bool locked)
{
    return locked ? "ON" : "OFF";
}

static bool mqtt_child_lock_publish(bool locked)
{
    const char *payload = mqtt_child_lock_payload(locked);

    if (s_child_lock_topic[0] == '\0') {
        return false;
    }

    if (s_last_payload_valid && strcmp(s_last_payload, payload) == 0) {
        return true;
    }

    if (!mqtt_outbox_enqueue(s_child_lock_topic, payload, strlen(payload), 1, true)) {
        app_log_debug("mqtt", "child lock enqueue failed topic=%s", s_child_lock_topic);
        return false;
    }

    (void)snprintf(s_last_payload, sizeof(s_last_payload), "%s", payload);
    s_last_payload_valid = true;
    return true;
}

static port_err_t mqtt_child_lock_parse_payload(const void *payload,
                                                size_t len,
                                                bool *locked_out)
{
    char buf[8];
    size_t start = 0u;
    size_t end;

    if (payload == NULL || locked_out == NULL || len == 0u || len >= sizeof(buf)) {
        return PORT_ERR_INVALID_ARG;
    }

    memcpy(buf, payload, len);
    buf[len] = '\0';

    while (buf[start] != '\0' && isspace((unsigned char)buf[start])) {
        start++;
    }

    end = strlen(buf + start);
    while (end > 0u && isspace((unsigned char)buf[start + end - 1u])) {
        buf[start + end - 1u] = '\0';
        end--;
    }

    if (strcmp(buf + start, "ON") == 0) {
        *locked_out = true;
        return PORT_OK;
    }

    if (strcmp(buf + start, "OFF") == 0) {
        *locked_out = false;
        return PORT_OK;
    }

    return PORT_ERR_INVALID_ARG;
}

void mqtt_child_lock_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0') {
        s_child_lock_topic[0] = '\0';
        return;
    }

    if (mqtt_topic_format(s_child_lock_topic,
                          sizeof(s_child_lock_topic),
                          device_id,
                          "child_lock")
            != PORT_OK) {
        s_child_lock_topic[0] = '\0';
    }
}

void mqtt_child_lock_publish_snapshot(void)
{
    (void)mqtt_child_lock_publish(feed_config_child_lock_is_active());
}

void mqtt_child_lock_connect_snapshot(void)
{
    s_last_payload_valid = false;
    mqtt_child_lock_publish_snapshot();
}

port_err_t mqtt_child_lock_apply(bool locked)
{
    if (feed_config_child_lock_is_active() == locked) {
        return PORT_OK;
    }

    if (!feed_config_child_lock_set(locked)) {
        return PORT_ERR_IO;
    }

    app_log_info("app", "child_lock %s", locked ? "on" : "off");
    mqtt_child_lock_publish_snapshot();
    return PORT_OK;
}

port_err_t mqtt_child_lock_handle(const void *payload, size_t len)
{
    bool locked;
    port_err_t err;

    err = mqtt_child_lock_parse_payload(payload, len, &locked);
    if (err != PORT_OK) {
        return err;
    }

    return mqtt_child_lock_apply(locked);
}

void mqtt_child_lock_test_reset(void)
{
    s_child_lock_topic[0] = '\0';
    s_last_payload[0] = '\0';
    s_last_payload_valid = false;
}
