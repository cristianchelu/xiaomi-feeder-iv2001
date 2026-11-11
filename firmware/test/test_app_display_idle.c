/* Tests: spec/30-processes/display-presentation.md § Idle blank on battery (app wiring) */

#include <string.h>

#include "unity.h"

#include "app.h"
#include "app_event.h"
#include "app_event_port.h"
#include "app_log.h"
#include "display_presentation.h"
#include "display_settings.h"
#include "fake_config_port.h"
#include "fake_display_port.h"
#include "fake_power_source_port.h"
#include "power_source_input.h"

extern void fake_app_event_q_reset(void);

static void tick(uint32_t now_ms)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = EVT_DISPLAY_TICK;
    ev.u.display_tick.now_ms = now_ms;
    TEST_ASSERT_TRUE(app_event_post(&ev));
    (void)app_step();
}

static void setup_battery(void)
{
    fake_config_port_reset();
    fake_display_port_reset();
    fake_app_event_q_reset();
    fake_power_source_port_reset();
    fake_power_source_port_set_mains_present(false);
    app_test_reset();
    display_presentation_reset();
    power_source_input_init(fake_power_source_port_get());
    app_display_apply_settings();
    tick(0u);
    tick(100u);
}

static bool last_op_is_power_off(void)
{
    size_t n;
    const fake_display_op_t *ops = fake_display_port_ops(&n);

    return n > 0u && ops[n - 1u].kind == FAKE_DISPLAY_OP_POWER_OFF;
}

void test_app_display_blanks_on_battery_after_timeout(void)
{
    setup_battery();
    tick(30000u);
    TEST_ASSERT_FALSE(last_op_is_power_off());
    tick(61000u);
    TEST_ASSERT_TRUE(last_op_is_power_off());
}

void test_app_display_idle_blank_disabled_when_timeout_zero(void)
{
    setup_battery();
    TEST_ASSERT_TRUE(display_settings_battery_off_s_set(0u));
    app_display_apply_settings();
    tick(500000u);
    TEST_ASSERT_FALSE(last_op_is_power_off());
}

void test_app_display_does_not_blank_on_mains(void)
{
    setup_battery();
    fake_power_source_port_set_mains_present(true);
    power_source_input_init(fake_power_source_port_get());
    tick(200000u);
    TEST_ASSERT_FALSE(last_op_is_power_off());
}

void test_app_display_blanks_on_mains_when_mains_delay_set(void)
{
    setup_battery();
    fake_power_source_port_set_mains_present(true);
    power_source_input_init(fake_power_source_port_get());
    TEST_ASSERT_TRUE(display_settings_mains_off_s_set(120u));
    app_display_apply_settings();
    tick(200u);
    tick(100000u);
    TEST_ASSERT_FALSE(last_op_is_power_off());
    tick(121000u);
    TEST_ASSERT_TRUE(last_op_is_power_off());
}

void test_app_logs_heap_every_ten_minutes(void)
{
    char log[3072];

    setup_battery();
    tick(1000u);
    tick(700000u);
    tick(1400000u);
    (void)app_log_ring_copy(log, sizeof(log));
    TEST_ASSERT_NOT_NULL(strstr(log, "[app] heap free "));
}
