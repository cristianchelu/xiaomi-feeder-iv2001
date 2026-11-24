/*
 * Daily limit and not-eaten alert — spec/30-processes/health-monitoring.md
 */

#include "feed_limit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_log.h"
#include "config_keys.h"
#include "config_port.h"
#include "eaten_today.h"
#include "feed_history.h"
#include "mqtt_json.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"
#include "problem.h"

#define LIMIT_KEY      "daily_max_g"
#define NOT_EATEN_KEY  "not_eaten_h"

static bool s_loaded;
static uint16_t s_max_g;
static uint8_t s_not_eaten_h;

/* Not-eaten tracking: eaten-today made cumulative across resets. */
static bool s_seen_total;
static weight_dg_t s_last_total;
static int32_t s_cum_offset;
static bool s_armed;
static uint32_t s_feed_ms;
static int32_t s_feed_cum;
static bool s_alerted;
static int32_t s_alert_cum;

static char s_topic[96];
static char s_last_payload[96];

static void feed_limit_load(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];
    long v;

    if (s_loaded) {
        return;
    }
    s_loaded = true;
    s_max_g = 0u;
    s_not_eaten_h = 0u;
    if (cfg == NULL) {
        return;
    }
    if (cfg->read(CONFIG_GROUP_FEED, LIMIT_KEY, buf, sizeof(buf)) == PORT_OK) {
        v = strtol(buf, NULL, 10);
        if (v >= (long)FEED_LIMIT_MIN_G && v <= (long)FEED_LIMIT_MAX_G) {
            s_max_g = (uint16_t)v;
        }
    }
    if (cfg->read(CONFIG_GROUP_FEED, NOT_EATEN_KEY, buf, sizeof(buf)) == PORT_OK) {
        v = strtol(buf, NULL, 10);
        if (v >= 1 && v <= (long)FEED_NOT_EATEN_MAX_H) {
            s_not_eaten_h = (uint8_t)v;
        }
    }
}

uint16_t feed_limit_daily_max_g(void)
{
    feed_limit_load();
    return s_max_g;
}

uint8_t feed_limit_not_eaten_h(void)
{
    feed_limit_load();
    return s_not_eaten_h;
}

static bool feed_limit_store(const char *key, unsigned v)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];

    if (cfg == NULL) {
        return false;
    }
    (void)snprintf(buf, sizeof(buf), "%u", v);
    return cfg->write(CONFIG_GROUP_FEED, key, buf) == PORT_OK;
}

bool feed_limit_set_daily_max_g(uint16_t g)
{
    feed_limit_load();
    if (g != 0u && (g < FEED_LIMIT_MIN_G || g > FEED_LIMIT_MAX_G)) {
        return false;
    }
    if (!feed_limit_store(LIMIT_KEY, g)) {
        return false;
    }
    s_max_g = g;
    return true;
}

bool feed_limit_set_not_eaten_h(uint8_t h)
{
    feed_limit_load();
    if (h > FEED_NOT_EATEN_MAX_H) {
        return false;
    }
    if (!feed_limit_store(NOT_EATEN_KEY, h)) {
        return false;
    }
    s_not_eaten_h = h;
    if (h == 0u) {
        s_armed = false;
        problem_clear(PROBLEM_NOT_EATEN);
    }
    return true;
}

bool feed_limit_remaining_g(uint32_t *remaining_g)
{
    uint32_t today;

    feed_limit_load();
    if (s_max_g == 0u) {
        return false;
    }
    today = feed_history_grams_on_day(eaten_today_day_key_now());
    if (remaining_g != NULL) {
        *remaining_g = today >= s_max_g ? 0u : (uint32_t)s_max_g - today;
    }
    return true;
}

static int32_t feed_not_eaten_cum(void)
{
    return s_cum_offset + (int32_t)s_last_total;
}

void feed_not_eaten_on_feed(weight_dg_t delivered_dg, uint32_t now_ms)
{
    if (delivered_dg < FEED_NOT_EATEN_MIN_DG) {
        return;
    }
    s_armed = true;
    s_feed_ms = now_ms;
    s_feed_cum = feed_not_eaten_cum();
}

void feed_not_eaten_poll(weight_dg_t eaten_today_dg, uint32_t now_ms)
{
    uint8_t h = feed_limit_not_eaten_h();

    if (!s_seen_total) {
        s_seen_total = true;
        s_last_total = eaten_today_dg;
    } else if (eaten_today_dg < s_last_total) {
        s_cum_offset += (int32_t)s_last_total;      /* midnight or manual reset */
        s_last_total = eaten_today_dg;
    } else {
        s_last_total = eaten_today_dg;
    }

    if (s_alerted && feed_not_eaten_cum() - s_alert_cum >= FEED_NOT_EATEN_MIN_DG) {
        s_alerted = false;
        problem_clear(PROBLEM_NOT_EATEN);
        app_log_info("health", "not eaten: cleared (eating detected)");
    }
    if (s_alerted && !problem_is_set(PROBLEM_NOT_EATEN)) {
        s_alerted = false;                            /* cleared by the button */
    }

    if (h == 0u || !s_armed) {
        return;
    }
    if (feed_not_eaten_cum() - s_feed_cum >= FEED_NOT_EATEN_MIN_DG) {
        s_armed = false;                              /* ate: nothing to watch */
        return;
    }
    if ((uint32_t)(now_ms - s_feed_ms) >= (uint32_t)h * 3600000u) {
        char detail[24];

        s_armed = false;
        s_alerted = true;
        s_alert_cum = feed_not_eaten_cum();
        (void)snprintf(detail, sizeof(detail), "%u h", (unsigned)h);
        problem_raise(PROBLEM_NOT_EATEN, detail);
        app_log_info("health", "not eaten %u h after the last meal", (unsigned)h);
    }
}

void mqtt_feed_limit_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "feed/limit") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

int mqtt_feed_limit_format(char *buf, size_t len)
{
    int n = snprintf(buf, len, "{\"daily_max_g\":%u,\"today_g\":%lu,\"not_eaten_h\":%u}",
                     (unsigned)feed_limit_daily_max_g(),
                     (unsigned long)feed_history_grams_on_day(eaten_today_day_key_now()),
                     (unsigned)feed_limit_not_eaten_h());

    return (n < 0 || (size_t)n >= len) ? -1 : n;
}

static void mqtt_feed_limit_publish(bool force)
{
    char payload[96];

    if (s_topic[0] == '\0' || mqtt_feed_limit_format(payload, sizeof(payload)) < 0) {
        return;
    }
    if (!force && strcmp(payload, s_last_payload) == 0) {
        return;
    }
    if (mqtt_outbox_enqueue(s_topic, payload, strlen(payload), 1, true)) {
        (void)snprintf(s_last_payload, sizeof(s_last_payload), "%s", payload);
    }
}

void mqtt_feed_limit_connect_snapshot(void)
{
    mqtt_feed_limit_publish(true);
}

void mqtt_feed_limit_sync(void)
{
    mqtt_feed_limit_publish(false);
}

port_err_t mqtt_feed_limit_handle(const void *payload, size_t len)
{
    unsigned v = 0u;
    bool any = false;

    if (payload == NULL || len == 0u) {
        return PORT_ERR_INVALID_ARG;
    }
    if (mqtt_json_find_uint(payload, len, "daily_max_g", &v)) {
        if (v > 0xFFFFu || !feed_limit_set_daily_max_g((uint16_t)v)) {
            return PORT_ERR_INVALID_ARG;
        }
        any = true;
    } else if (mqtt_json_has_key(payload, len, "daily_max_g")) {
        return PORT_ERR_INVALID_ARG;
    }
    if (mqtt_json_find_uint(payload, len, "not_eaten_h", &v)) {
        if (v > 0xFFu || !feed_limit_set_not_eaten_h((uint8_t)v)) {
            return PORT_ERR_INVALID_ARG;
        }
        any = true;
    } else if (mqtt_json_has_key(payload, len, "not_eaten_h")) {
        return PORT_ERR_INVALID_ARG;
    }
    if (!any) {
        return PORT_ERR_INVALID_ARG;
    }
    mqtt_feed_limit_publish(false);
    return PORT_OK;
}

void feed_limit_test_reset(void)
{
    s_loaded = false;
    s_seen_total = false;
    s_last_total = 0;
    s_cum_offset = 0;
    s_armed = false;
    s_alerted = false;
    s_topic[0] = '\0';
    s_last_payload[0] = '\0';
}
