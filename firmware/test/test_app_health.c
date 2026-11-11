/* Tests: spec/30-processes/health-monitoring.md § Problem state (E4, E5, panel) */

#include <string.h>

#include "unity.h"

#include "app_health.h"
#include "fake_config_port.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "problem.h"
#include "task_watch.h"

static void setup(void)
{
    fake_config_port_reset();
    feed_history_test_reset();
    feed_limit_test_reset();
    problem_test_reset();
    task_watch_test_reset();
    app_health_test_reset();
}

void test_app_health_panel_cycles_err_code_and_normal(void)
{
    uint8_t s[3];

    TEST_ASSERT_TRUE(app_health_problem_segments(1, 0u, s));
    TEST_ASSERT_EQUAL_HEX8(0x79, s[0]);
    TEST_ASSERT_EQUAL_HEX8(0x50, s[1]);
    TEST_ASSERT_TRUE(app_health_problem_segments(1, 1600u, s));
    TEST_ASSERT_EQUAL_HEX8(0x79, s[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, s[1]);
    TEST_ASSERT_EQUAL_HEX8(0x06, s[2]);                 /* "1" */
    TEST_ASSERT_FALSE(app_health_problem_segments(1, 3100u, s));
    TEST_ASSERT_FALSE(app_health_problem_segments(1, 5900u, s));
    TEST_ASSERT_TRUE(app_health_problem_segments(1, 6000u, s));
    TEST_ASSERT_FALSE(app_health_problem_segments(0, 0u, s));
}

void test_app_health_connection_lost_after_five_minutes(void)
{
    app_health_inputs_t in = { .mqtt_up = true, .mqtt_armed = true, .panel_free = false };

    setup();
    app_health_tick(1000u, &in);
    in.mqtt_up = false;
    app_health_tick(2000u, &in);
    TEST_ASSERT_EQUAL_UINT32(1u, feed_counter_get(FEED_COUNTER_MQTT_LOST));
    app_health_tick(2000u + APP_HEALTH_MQTT_LOST_MS - 1u, &in);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_CONNECTION));
    app_health_tick(2000u + APP_HEALTH_MQTT_LOST_MS, &in);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_CONNECTION));
    in.mqtt_up = true;
    app_health_tick(2000u + 12u * 60000u, &in);
    {
        char text[128];

        problem_format_text(text, sizeof(text));
        TEST_ASSERT_NOT_NULL(strstr(text, "Connection lost (12 min)"));
    }
}

void test_app_health_short_outage_is_not_a_problem(void)
{
    app_health_inputs_t in = { .mqtt_up = true, .mqtt_armed = true, .panel_free = false };

    setup();
    app_health_tick(1000u, &in);
    in.mqtt_up = false;
    app_health_tick(2000u, &in);
    in.mqtt_up = true;
    app_health_tick(60000u, &in);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_CONNECTION));
}

void test_app_health_time_problem_after_boot_grace(void)
{
    app_health_inputs_t in = { .mqtt_up = true, .mqtt_armed = true, .panel_free = false };

    setup();
    app_health_tick(1000u, &in);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_TIME));
    app_health_tick(1000u + APP_HEALTH_TIME_GRACE_MS, &in);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_TIME));      /* clock never synced in tests */
}
