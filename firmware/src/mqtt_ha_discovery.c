/*
 * Home Assistant MQTT discovery — spec/30-processes/mqtt-protocol.md
 */

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "mqtt_ha_discovery.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "port_err.h"

typedef struct {
    const char *component;
    const char *object_id;
    int (*format_config)(char *buf, size_t len, const char *device_id);
} mqtt_ha_entity_t;

static int mqtt_ha_format_device_suffix(char *buf,
                                        size_t len,
                                        const char *device_id)
{
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "\"device\":{\"identifiers\":[\"petfeeder_%s\"],"
                       "\"name\":\"Pet Feeder %s\","
                       "\"manufacturer\":\"%s\","
                       "\"model\":\"%s\"}",
                       device_id,
                       device_id,
                       MQTT_HA_MANUFACTURER,
                       MQTT_HA_MODEL);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_dispense_button_config(char *buf,
                                          size_t len,
                                          const char *device_id)
{
    char cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(cmd_topic, sizeof(cmd_topic), device_id, "cmd/dispense")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Dispense\","
                       "\"unique_id\":\"petfeeder_%s_dispense\","
                       "\"command_topic\":\"%s\","
                       "\"payload_press\":\"{}\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       cmd_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_bowl_error_config(char *buf, size_t len, const char *device_id)
{
    char state_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, "state") != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Bowl error\","
                       "\"unique_id\":\"petfeeder_%s_bowl_error\","
                       "\"state_topic\":\"%s\","
                       "\"value_template\":\"{{ value_json.bowl_error }}\","
                       "\"payload_on\":true,"
                       "\"payload_off\":false,"
                       "\"device_class\":\"problem\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       state_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_bowl_weight_config(char *buf, size_t len, const char *device_id)
{
    char bowl_weight_topic[96];
    char state_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(bowl_weight_topic,
                          sizeof(bowl_weight_topic),
                          device_id,
                          "bowl_weight")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, "state") != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Bowl weight\","
                       "\"unique_id\":\"petfeeder_%s_bowl_weight\","
                       "\"state_topic\":\"%s\","
                       "\"unit_of_measurement\":\"g\","
                       "\"device_class\":\"weight\","
                       "\"state_class\":\"measurement\","
                       "\"availability\":["
                       "{\"topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\"},"
                       "{\"topic\":\"%s\","
                       "\"value_template\":\"{{ value_json.bowl_error == false }}\","
                       "\"payload_available\":\"true\","
                       "\"payload_not_available\":\"false\"}"
                       "],"
                       "%s}",
                       device_id,
                       bowl_weight_topic,
                       connection_topic,
                       state_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_battery_config(char *buf, size_t len, const char *device_id)
{
    char battery_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(battery_topic,
                          sizeof(battery_topic),
                          device_id,
                          "battery")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Battery\","
                       "\"unique_id\":\"petfeeder_%s_battery\","
                       "\"state_topic\":\"%s\","
                       "\"unit_of_measurement\":\"%%\","
                       "\"device_class\":\"battery\","
                       "\"state_class\":\"measurement\","
                       "\"availability\":["
                       "{\"topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\"},"
                       "{\"topic\":\"%s\","
                       "\"value_template\":\"{{ 'true' if value != 'unknown' else 'false' }}\","
                       "\"payload_available\":\"true\","
                       "\"payload_not_available\":\"false\"}"
                       "],"
                       "%s}",
                       device_id,
                       battery_topic,
                       connection_topic,
                       battery_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_battery_voltage_config(char *buf, size_t len, const char *device_id)
{
    char voltage_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(voltage_topic,
                          sizeof(voltage_topic),
                          device_id,
                          "battery_voltage")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Battery pack voltage\","
                       "\"unique_id\":\"petfeeder_%s_battery_voltage\","
                       "\"state_topic\":\"%s\","
                       "\"unit_of_measurement\":\"mV\","
                       "\"device_class\":\"voltage\","
                       "\"state_class\":\"measurement\","
                       "\"entity_category\":\"diagnostic\","
                       "\"enabled_by_default\":false,"
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       voltage_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_mains_config(char *buf, size_t len, const char *device_id)
{
    char mains_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(mains_topic, sizeof(mains_topic), device_id, "mains")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Mains connected\","
                       "\"unique_id\":\"petfeeder_%s_mains\","
                       "\"state_topic\":\"%s\","
                       "\"payload_on\":\"ON\","
                       "\"payload_off\":\"OFF\","
                       "\"device_class\":\"power\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       mains_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_hopper_level_config(char *buf, size_t len, const char *device_id)
{
    char hopper_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(hopper_topic, sizeof(hopper_topic), device_id, "hopper")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Hopper level\","
                       "\"unique_id\":\"petfeeder_%s_hopper_level\","
                       "\"state_topic\":\"%s\","
                       "\"device_class\":\"enum\","
                       "\"options\":[\"normal\",\"low\",\"empty\"],"
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       hopper_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_dispense_completed_config(char *buf,
                                             size_t len,
                                             const char *device_id)
{
    char event_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(event_topic,
                          sizeof(event_topic),
                          device_id,
                          "dispense/event")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Dispense completed\","
                       "\"unique_id\":\"petfeeder_%s_dispense_completed\","
                       "\"state_topic\":\"%s\","
                       "\"event_types\":[\"success\",\"underfill\",\"stuck\",\"empty_hopper\",\"aborted\"],"
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       event_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_device_timezone_config(char *buf, size_t len, const char *device_id)
{
    char timezone_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(timezone_topic,
                          sizeof(timezone_topic),
                          device_id,
                          "timezone")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Device timezone\","
                       "\"unique_id\":\"petfeeder_%s_device_timezone\","
                       "\"state_topic\":\"%s\","
                       "\"entity_category\":\"diagnostic\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       timezone_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_feeding_schedule_config(char *buf, size_t len, const char *device_id)
{
    char schedule_topic[96];
    char schedule_cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(schedule_topic,
                          sizeof(schedule_topic),
                          device_id,
                          "schedule/state")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(schedule_cmd_topic,
                          sizeof(schedule_cmd_topic),
                          device_id,
                          "cmd/schedule/enable")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Feeding schedule\","
                       "\"unique_id\":\"petfeeder_%s_feeding_schedule\","
                       "\"state_topic\":\"%s\","
                       "\"value_template\":\"{{ 'ON' if value_json.enabled else 'OFF' }}\","
                       "\"state_on\":\"ON\","
                       "\"state_off\":\"OFF\","
                       "\"command_topic\":\"%s\","
                       "\"payload_on\":\"{\\\"enabled\\\": true}\","
                       "\"payload_off\":\"{\\\"enabled\\\": false}\","
                       "\"json_attributes_topic\":\"%s\","
                       "\"json_attributes_template\":\"{{ value_json | tojson }}\","
                       "\"force_update\":true,"
                       "\"optimistic\":false,"
                       "\"entity_category\":\"config\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       schedule_topic,
                       schedule_cmd_topic,
                       schedule_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_weight_compensation_config(char *buf, size_t len, const char *device_id)
{
    char feed_mode_topic[96];
    char feed_mode_cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(feed_mode_topic,
                          sizeof(feed_mode_topic),
                          device_id,
                          "feed/mode")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(feed_mode_cmd_topic,
                          sizeof(feed_mode_cmd_topic),
                          device_id,
                          "cmd/feed/mode")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block,
                                     sizeof(device_block),
                                     device_id)
            < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Weight compensation\","
                       "\"unique_id\":\"petfeeder_%s_weight_compensation\","
                       "\"state_topic\":\"%s\","
                       "\"value_template\":\"{{ 'ON' if value == 'compensated' else 'OFF' }}\","
                       "\"state_on\":\"ON\","
                       "\"state_off\":\"OFF\","
                       "\"command_topic\":\"%s\","
                       "\"payload_on\":\"compensated\","
                       "\"payload_off\":\"open_loop\","
                       "\"optimistic\":false,"
                       "\"entity_category\":\"config\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       feed_mode_topic,
                       feed_mode_cmd_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_child_lock_config(char *buf, size_t len, const char *device_id)
{
    char state_topic[96];
    char cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, "child_lock")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(cmd_topic, sizeof(cmd_topic), device_id, "cmd/child_lock")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block, sizeof(device_block), device_id) < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Child lock\","
                       "\"unique_id\":\"petfeeder_%s_child_lock\","
                       "\"state_topic\":\"%s\","
                       "\"state_on\":\"ON\","
                       "\"state_off\":\"OFF\","
                       "\"command_topic\":\"%s\","
                       "\"payload_on\":\"ON\","
                       "\"payload_off\":\"OFF\","
                       "\"optimistic\":false,"
                       "\"entity_category\":\"config\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       state_topic,
                       cmd_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_overfill_protection_config(char *buf, size_t len, const char *device_id)
{
    char state_topic[96];
    char cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, "feed/overfill")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(cmd_topic, sizeof(cmd_topic), device_id, "cmd/feed/overfill")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block, sizeof(device_block), device_id) < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Overfill protection\","
                       "\"unique_id\":\"petfeeder_%s_overfill_protection\","
                       "\"state_topic\":\"%s\","
                       "\"value_template\":\"{{ 'ON' if value_json.enabled else 'OFF' }}\","
                       "\"state_on\":\"ON\","
                       "\"state_off\":\"OFF\","
                       "\"command_topic\":\"%s\","
                       "\"payload_on\":\"{\\\"enabled\\\": true}\","
                       "\"payload_off\":\"{\\\"enabled\\\": false}\","
                       "\"optimistic\":false,"
                       "\"entity_category\":\"config\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       state_topic,
                       cmd_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

int mqtt_ha_format_overfill_threshold_g_config(char *buf, size_t len, const char *device_id)
{
    char state_topic[96];
    char cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }

    if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, "feed/overfill")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(cmd_topic, sizeof(cmd_topic), device_id, "cmd/feed/overfill")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_topic_format(connection_topic,
                          sizeof(connection_topic),
                          device_id,
                          "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block, sizeof(device_block), device_id) < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"Overfill threshold\","
                       "\"unique_id\":\"petfeeder_%s_overfill_threshold_g\","
                       "\"state_topic\":\"%s\","
                       "\"value_template\":\"{{ value_json.threshold_g }}\","
                       "\"command_topic\":\"%s\","
                       "\"command_template\":\"{\\\"threshold_g\\\": {{ value }}}\","
                       "\"min\":30,"
                       "\"max\":100,"
                       "\"step\":1,"
                       "\"unit_of_measurement\":\"g\","
                       "\"mode\":\"box\","
                       "\"optimistic\":false,"
                       "\"entity_category\":\"config\","
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       device_id,
                       state_topic,
                       cmd_topic,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}


/* Table-driven entities (no per-entity boilerplate). */
typedef struct {
    const char *name;
    const char *object_id;
    const char *state_suffix;   /* NULL = no state_topic */
    const char *cmd_suffix;     /* NULL = no command_topic */
    const char *extra;          /* JSON members, no braces, no trailing comma */
} mqtt_ha_generic_t;

static int mqtt_ha_format_generic(char *buf,
                                  size_t len,
                                  const char *device_id,
                                  const mqtt_ha_generic_t *g)
{
    char state_member[128] = "";
    char cmd_member[128] = "";
    char state_topic[96];
    char cmd_topic[96];
    char connection_topic[96];
    char device_block[192];
    int written;

    if (buf == NULL || len == 0 || device_id == NULL || device_id[0] == '\0') {
        return -1;
    }
    if (g->state_suffix != NULL) {
        if (mqtt_topic_format(state_topic, sizeof(state_topic), device_id, g->state_suffix)
                != PORT_OK) {
            return -1;
        }
        (void)snprintf(state_member, sizeof(state_member), "\"state_topic\":\"%s\",", state_topic);
    }
    if (g->cmd_suffix != NULL) {
        if (mqtt_topic_format(cmd_topic, sizeof(cmd_topic), device_id, g->cmd_suffix) != PORT_OK) {
            return -1;
        }
        (void)snprintf(cmd_member, sizeof(cmd_member), "\"command_topic\":\"%s\",", cmd_topic);
    }
    if (mqtt_topic_format(connection_topic, sizeof(connection_topic), device_id, "connection")
            != PORT_OK) {
        return -1;
    }
    if (mqtt_ha_format_device_suffix(device_block, sizeof(device_block), device_id) < 0) {
        return -1;
    }

    written = snprintf(buf,
                       len,
                       "{\"name\":\"%s\","
                       "\"unique_id\":\"petfeeder_%s_%s\","
                       "%s%s%s,"
                       "\"availability_topic\":\"%s\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "%s}",
                       g->name,
                       device_id,
                       g->object_id,
                       state_member,
                       cmd_member,
                       g->extra,
                       connection_topic,
                       device_block);
    if (written < 0 || (size_t)written >= len) {
        return -1;
    }
    return written;
}

#define MQTT_HA_GENERIC(fn, ...)                                              \
    int fn(char *buf, size_t len, const char *device_id)                      \
    {                                                                         \
        static const mqtt_ha_generic_t spec = { __VA_ARGS__ };                \
        return mqtt_ha_format_generic(buf, len, device_id, &spec);            \
    }

MQTT_HA_GENERIC(mqtt_ha_format_reboot_config,
    "Reboot", "reboot", NULL, "cmd/reboot",
    "\"payload_press\":\"{}\",\"device_class\":\"restart\",\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_calibrate_zero_config,
    "Calibrate zero (bowl removed)", "calibrate_zero", NULL, "cmd/calibrate",
    "\"payload_press\":\"{\\\"action\\\":\\\"zero\\\"}\",\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_calibrate_span_config,
    "Calibrate span (empty 350 g bowl)", "calibrate_span", NULL, "cmd/calibrate",
    "\"payload_press\":\"{\\\"action\\\":\\\"span\\\"}\",\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_display_mode_config,
    "Display mode", "display_mode", "display", "cmd/display",
    "\"value_template\":\"{{ value_json.mode }}\","
    "\"command_template\":\"{\\\"mode\\\": \\\"{{ value }}\\\"}\","
    "\"options\":[\"weight\",\"eaten_today\",\"off\"],"
    "\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_display_brightness_config,
    "Display brightness", "display_brightness", "display", "cmd/display",
    "\"value_template\":\"{{ value_json.brightness }}\","
    "\"command_template\":\"{\\\"brightness\\\": {{ value | int }}}\","
    "\"min\":1,\"max\":4,\"step\":1,\"mode\":\"slider\",\"optimistic\":false,"
    "\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_display_battery_off_config,
    "Display off delay on battery", "display_battery_off", "display", "cmd/display",
    "\"value_template\":\"{{ value_json.battery_off_s }}\","
    "\"command_template\":\"{\\\"battery_off_s\\\": {{ value | int }}}\","
    "\"min\":0,\"max\":600,\"step\":10,\"unit_of_measurement\":\"s\","
    "\"mode\":\"box\",\"optimistic\":false,\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_eaten_today_config,
    "Eaten today", "eaten_today", "eaten_today", NULL,
    "\"device_class\":\"weight\",\"unit_of_measurement\":\"g\","
    "\"state_class\":\"total_increasing\"")

MQTT_HA_GENERIC(mqtt_ha_format_dispense_custom_config,
    "Dispense custom amount", "dispense_custom", NULL, "cmd/dispense",
    "\"command_template\":\"{\\\"g\\\": {{ value | int }}}\","
    "\"min\":5,\"max\":150,\"step\":1,\"unit_of_measurement\":\"g\","
    "\"mode\":\"box\",\"optimistic\":true,\"entity_category\":\"config\"")

MQTT_HA_GENERIC(mqtt_ha_format_hopper_ir_config,
    "Hopper IR beam", "hopper_ir", "hopper_ir", NULL,
    "\"device_class\":\"enum\",\"options\":[\"blocked\",\"clear\"],"
    "\"entity_category\":\"diagnostic\"")

MQTT_HA_GENERIC(mqtt_ha_format_ota_state_config,
    "Firmware update state", "ota_state", "ota/status", NULL,
    "\"value_template\":\"{{ value_json.state }}\","
    "\"entity_category\":\"diagnostic\"")

MQTT_HA_GENERIC(mqtt_ha_format_firmware_bank_config,
    "Firmware bank", "firmware_bank", "ota/status", NULL,
    "\"value_template\":\"{{ value_json.bank }}\",\"entity_category\":\"diagnostic\"")

static const mqtt_ha_entity_t s_ha_entities[] = {
    {
        .component = "button",
        .object_id = "dispense",
        .format_config = mqtt_ha_format_dispense_button_config,
    },
    {
        .component = "binary_sensor",
        .object_id = "bowl_error",
        .format_config = mqtt_ha_format_bowl_error_config,
    },
    {
        .component = "sensor",
        .object_id = "bowl_weight",
        .format_config = mqtt_ha_format_bowl_weight_config,
    },
    {
        .component = "sensor",
        .object_id = "battery",
        .format_config = mqtt_ha_format_battery_config,
    },
    {
        .component = "sensor",
        .object_id = "battery_voltage",
        .format_config = mqtt_ha_format_battery_voltage_config,
    },
    {
        .component = "binary_sensor",
        .object_id = "mains",
        .format_config = mqtt_ha_format_mains_config,
    },
    {
        .component = "sensor",
        .object_id = "hopper_level",
        .format_config = mqtt_ha_format_hopper_level_config,
    },
    {
        .component = "sensor",
        .object_id = "device_timezone",
        .format_config = mqtt_ha_format_device_timezone_config,
    },
    {
        .component = "switch",
        .object_id = "feeding_schedule",
        .format_config = mqtt_ha_format_feeding_schedule_config,
    },
    {
        .component = "event",
        .object_id = "dispense_completed",
        .format_config = mqtt_ha_format_dispense_completed_config,
    },
    {
        .component = "switch",
        .object_id = "weight_compensation",
        .format_config = mqtt_ha_format_weight_compensation_config,
    },
    {
        .component = "switch",
        .object_id = "overfill_protection",
        .format_config = mqtt_ha_format_overfill_protection_config,
    },
    {
        .component = "number",
        .object_id = "overfill_threshold_g",
        .format_config = mqtt_ha_format_overfill_threshold_g_config,
    },
    {
        .component = "switch",
        .object_id = "child_lock",
        .format_config = mqtt_ha_format_child_lock_config,
    },
    {
        .component = "button",
        .object_id = "reboot",
        .format_config = mqtt_ha_format_reboot_config,
    },
    {
        .component = "button",
        .object_id = "calibrate_zero",
        .format_config = mqtt_ha_format_calibrate_zero_config,
    },
    {
        .component = "button",
        .object_id = "calibrate_span",
        .format_config = mqtt_ha_format_calibrate_span_config,
    },
    {
        .component = "select",
        .object_id = "display_mode",
        .format_config = mqtt_ha_format_display_mode_config,
    },
    {
        .component = "number",
        .object_id = "display_brightness",
        .format_config = mqtt_ha_format_display_brightness_config,
    },
    {
        .component = "number",
        .object_id = "display_battery_off",
        .format_config = mqtt_ha_format_display_battery_off_config,
    },
    {
        .component = "sensor",
        .object_id = "eaten_today",
        .format_config = mqtt_ha_format_eaten_today_config,
    },
    {
        .component = "number",
        .object_id = "dispense_custom",
        .format_config = mqtt_ha_format_dispense_custom_config,
    },
    {
        .component = "sensor",
        .object_id = "hopper_ir",
        .format_config = mqtt_ha_format_hopper_ir_config,
    },
    {
        .component = "sensor",
        .object_id = "ota_state",
        .format_config = mqtt_ha_format_ota_state_config,
    },
    {
        .component = "sensor",
        .object_id = "firmware_bank",
        .format_config = mqtt_ha_format_firmware_bank_config,
    },
};

static bool mqtt_ha_enqueue_entity(const mqtt_ha_entity_t *entity,
                                   const char *device_id)
{
    char topic[128];
    char payload[MQTT_OUTBOX_MAX_PAYLOAD];
    int written;

    if (entity == NULL || device_id == NULL || device_id[0] == '\0') {
        return false;
    }

    written = snprintf(topic,
                       sizeof(topic),
                       "homeassistant/%s/petfeeder_%s/%s/config",
                       entity->component,
                       device_id,
                       entity->object_id);
    if (written < 0 || (size_t)written >= sizeof(topic)) {
        return false;
    }

    written = entity->format_config(payload, sizeof(payload), device_id);
    if (written < 0) {
        return false;
    }

    if (!mqtt_outbox_enqueue(topic, payload, (size_t)written, 1, true)) {
        app_log_debug("mqtt", "ha discovery enqueue failed topic=%s", topic);
        return false;
    }

    return true;
}

static char s_discovery_device_id[MQTT_DEVICE_ID_MAX_LEN + 1];
static size_t s_discovery_next;
static bool s_discovery_armed;

#define HA_ENTITY_COUNT  (sizeof(s_ha_entities) / sizeof(s_ha_entities[0]))

unsigned mqtt_ha_discovery_entity_count(void)
{
    return (unsigned)HA_ENTITY_COUNT;
}

void mqtt_ha_discovery_test_reset(void)
{
    s_discovery_device_id[0] = '\0';
    s_discovery_next = 0u;
    s_discovery_armed = false;
}

void mqtt_ha_discovery_disarm(void)
{
    s_discovery_armed = false;
}

void mqtt_ha_discovery_schedule(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        strlen(device_id) > MQTT_DEVICE_ID_MAX_LEN) {
        return;
    }

    strcpy(s_discovery_device_id, device_id);
    s_discovery_next = 0u;
    s_discovery_armed = true;
}

unsigned mqtt_ha_discovery_remaining(void)
{
    if (!s_discovery_armed) {
        return 0u;
    }

    return (unsigned)(HA_ENTITY_COUNT - s_discovery_next);
}

bool mqtt_ha_discovery_pump(void)
{
    if (!s_discovery_armed) {
        return false;
    }

    if (!mqtt_outbox_is_accepting()) {
        s_discovery_armed = false;
        return false;
    }

    if (mqtt_outbox_pending() >= MQTT_HA_DISCOVERY_MAX_PENDING) {
        return false;
    }

    if (!mqtt_ha_enqueue_entity(&s_ha_entities[s_discovery_next], s_discovery_device_id)) {
        return false;
    }

    s_discovery_next++;
    if (s_discovery_next >= HA_ENTITY_COUNT) {
        s_discovery_armed = false;
    }

    return true;
}
