/*
 * Health tick — spec/30-processes/health-monitoring.md
 */

#include "app_health.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "display_glyph.h"
#include "display_presentation.h"
#include "eaten_today.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "feeder_runtime.h"
#include "hopper_level.h"
#include "mqtt_problem.h"
#include "problem.h"
#include "task_watch.h"
#include "time_sync.h"

#define SEG_E 0x79u
#define SEG_R 0x50u

static bool s_mqtt_was_up;
static bool s_down_tracking;
static uint32_t s_down_since_ms;
static bool s_e4_raised;
static uint32_t s_day;
static bool s_started;
static uint32_t s_start_ms;   /* first tick after boot */

bool app_health_problem_segments(int code, uint32_t now_ms, uint8_t out[3])
{
    uint32_t phase = (now_ms % 6000u) / 1500u;   /* 0 Err, 1 E<n>, 2-3 normal */

    if (code <= 0 || code > 9 || out == NULL || phase >= 2u) {
        return false;
    }
    if (phase == 0u) {
        out[0] = SEG_E;
        out[1] = SEG_R;
        out[2] = SEG_R;
    } else {
        out[0] = SEG_E;
        out[1] = 0x00u;
        out[2] = display_glyph_digit_segment((uint8_t)code);
    }
    return true;
}

static void app_health_mqtt(uint32_t now_ms, const app_health_inputs_t *in)
{
    if (in->mqtt_up) {
        if (s_down_tracking && s_e4_raised && problem_is_set(PROBLEM_CONNECTION)) {
            char detail[24];

            (void)snprintf(detail, sizeof(detail), "%lu min",
                           (unsigned long)((now_ms - s_down_since_ms) / 60000u));
            problem_raise(PROBLEM_CONNECTION, detail);
        }
        s_down_tracking = false;
        s_e4_raised = false;
        s_mqtt_was_up = true;
        return;
    }
    if (!in->mqtt_armed) {
        return;
    }
    if (s_mqtt_was_up) {
        s_mqtt_was_up = false;
        feed_counter_inc(FEED_COUNTER_MQTT_LOST);
        s_down_tracking = true;
        s_down_since_ms = now_ms;
    }
    if (!s_down_tracking) {
        s_down_tracking = true;
        s_down_since_ms = now_ms;
    }
    if (!s_e4_raised && (uint32_t)(now_ms - s_down_since_ms) >= APP_HEALTH_MQTT_LOST_MS) {
        s_e4_raised = true;
        problem_raise(PROBLEM_CONNECTION, NULL);
        app_log_info("health", "connection lost for %lu min",
                     (unsigned long)((now_ms - s_down_since_ms) / 60000u));
    }
}

void app_health_tick(uint32_t now_ms, const app_health_inputs_t *in)
{
    uint8_t segs[3];
    uint32_t day;

    if (in == NULL) {
        return;
    }
    if (!s_started) {
        s_started = true;
        s_start_ms = now_ms;
    }
    if (!feeder_runtime_ota_active()) {   /* OTA hands the radio and CPU to the updater */
        (void)task_watch_check(TASK_WATCH_MOTOR, now_ms);
        (void)task_watch_check(TASK_WATCH_MQTT, now_ms);
    }

    problem_set_live(PROBLEM_HOPPER_EMPTY, hopper_level_get() == HOPPER_LEVEL_STATE_EMPTY);
    problem_set_live(PROBLEM_TIME, (uint32_t)(now_ms - s_start_ms) >= APP_HEALTH_TIME_GRACE_MS &&
                                       !time_sync_is_valid());
    app_health_mqtt(now_ms, in);

    day = eaten_today_day_key_now();
    if (day != 0u && s_day != 0u && day != s_day) {
        problem_clear(PROBLEM_DAILY_LIMIT);
    }
    if (day != 0u) {
        s_day = day;
    }

    feed_not_eaten_poll(eaten_today_total_dg(), now_ms);
    mqtt_problem_sync();
    mqtt_feed_limit_sync();

    if (in->panel_free && app_health_problem_segments((int)problem_top(), now_ms, segs)) {
        (void)display_presentation_set_overlay(segs);
    } else {
        (void)display_presentation_set_overlay(NULL);
    }
}

void app_health_test_reset(void)
{
    s_mqtt_was_up = false;
    s_down_tracking = false;
    s_down_since_ms = 0u;
    s_e4_raised = false;
    s_day = 0u;
    s_started = false;
    s_start_ms = 0u;
}
