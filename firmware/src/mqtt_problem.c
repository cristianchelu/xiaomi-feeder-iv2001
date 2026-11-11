/*
 * Problem state over MQTT — spec/30-processes/health-monitoring.md § Problem state
 */

#include "mqtt_problem.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "feed_history.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "problem.h"

#define MQTT_PROBLEM_PAYLOAD_MAX 768u

static char s_topic[96];
static char s_last[MQTT_PROBLEM_PAYLOAD_MAX];

void mqtt_problem_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "problem") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

int mqtt_problem_format(char *buf, size_t len)
{
    char codes[64];
    char text[256];
    feed_record_t r;
    int n;

    if (buf == NULL || len == 0u) {
        return -1;
    }
    if (problem_format_codes(codes, sizeof(codes)) < 0) {
        return -1;
    }
    (void)problem_format_text(text, sizeof(text));
    n = snprintf(buf, len, "{\"active\":%s,\"codes\":%s,\"text\":\"%s\"",
                 problem_active() ? "true" : "false", codes, text);
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    if (feed_history_get(0u, &r)) {
        int m;

        if ((size_t)n + 14u >= len) {
            return -1;
        }
        memcpy(buf + n, ",\"last_feed\":", 13u);
        n += 13;
        m = feed_history_format_record(&r, buf + n, len - (size_t)n);
        if (m < 0) {
            return -1;
        }
        n += m;
    }
    if ((size_t)n + 2u > len) {
        return -1;
    }
    buf[n++] = '}';
    buf[n] = '\0';
    return n;
}

static void mqtt_problem_publish(bool force)
{
    char payload[MQTT_PROBLEM_PAYLOAD_MAX];

    if (s_topic[0] == '\0' || mqtt_problem_format(payload, sizeof(payload)) < 0) {
        return;
    }
    if (!force && strcmp(payload, s_last) == 0) {
        return;
    }
    if (mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 1, true)) {
        (void)snprintf(s_last, sizeof(s_last), "%s", payload);
    }
}

void mqtt_problem_sync(void)
{
    mqtt_problem_publish(false);
}

void mqtt_problem_connect_snapshot(void)
{
    mqtt_problem_publish(true);
}

port_err_t mqtt_problem_handle_clear(void)
{
    problem_clear_latched();
    app_log_info("health", "problems cleared");
    mqtt_problem_publish(false);
    return PORT_OK;
}

void mqtt_problem_test_reset(void)
{
    s_topic[0] = '\0';
    s_last[0] = '\0';
}
