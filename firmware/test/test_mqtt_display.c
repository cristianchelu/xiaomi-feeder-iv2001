/* Tests: spec/30-processes/mqtt-protocol.md § Display */

#include <string.h>

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "display_settings.h"
#include "fake_config_port.h"
#include "fake_display_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "mqtt_display.h"
#include "mqtt_outbox.h"
#include "display_presentation.h"
#include "mqtt_route.h"

#define TEST_DEVICE_ID "ddeeff"

static void drain_outbox(void)
{
    const mqtt_port_t *mqtt = fake_mqtt_port_get();

    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(mqtt)) {
            fake_time_advance_ms(101u);
            (void)mqtt_outbox_drain_one(mqtt);
        }
    }
}

static void setup(void)
{
    fake_config_port_reset();
    fake_mqtt_port_reset();
    fake_time_reset();
    fake_display_port_reset();
    display_presentation_reset();
    mqtt_outbox_reset();
    mqtt_display_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    mqtt_display_set_device_id(TEST_DEVICE_ID);
}

static port_err_t cmd(const char *json)
{
    return app_cmd_dispatch(MQTT_ROUTE_CMD_DISPLAY, json, strlen(json), TEST_DEVICE_ID);
}

void test_mqtt_display_connect_snapshot_publishes_defaults_retained(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    mqtt_display_connect_snapshot();
    drain_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/display", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("{\"mode\":\"weight\",\"brightness\":4}", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}

void test_mqtt_display_cmd_off_persists_blanks_and_publishes(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"mode\":\"off\"}"));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_OFF, display_settings_mode_get());
    drain_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("{\"mode\":\"off\",\"brightness\":4}", mqtt->last_publish_payload);
}

void test_mqtt_display_cmd_brightness_only_keeps_mode(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"brightness\":2}"));
    TEST_ASSERT_EQUAL_UINT8(2u, display_settings_brightness_get());
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
}

void test_mqtt_display_cmd_both_keys_apply_together(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"mode\":\"off\",\"brightness\":1}"));
    drain_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("{\"mode\":\"off\",\"brightness\":1}", mqtt->last_publish_payload);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
}

void test_mqtt_display_cmd_invalid_key_rejects_whole_command(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"mode\":\"off\",\"brightness\":9}"));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
    TEST_ASSERT_EQUAL_UINT8(4u, display_settings_brightness_get());
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
}

void test_mqtt_display_cmd_unknown_mode_rejected(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"mode\":\"sparkle\"}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"mode\":\"eaten_today\"}"));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
}

void test_mqtt_display_cmd_empty_or_keyless_payload_rejected(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd(""));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("off"));
}

void test_mqtt_display_cmd_no_change_succeeds_without_publish(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"mode\":\"weight\",\"brightness\":4}"));
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
}

void test_mqtt_display_cmd_off_powers_panel_down_and_weight_restores(void)
{
    size_t n;
    const fake_display_op_t *ops;

    setup();
    (void)display_presentation_set_digits(42u);
    (void)display_presentation_refresh();

    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"mode\":\"off\"}"));
    ops = fake_display_port_ops(&n);
    TEST_ASSERT_TRUE(n > 0u);
    TEST_ASSERT_EQUAL(FAKE_DISPLAY_OP_POWER_OFF, ops[n - 1u].kind);

    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"mode\":\"weight\"}"));
    ops = fake_display_port_ops(&n);
    TEST_ASSERT_EQUAL(FAKE_DISPLAY_OP_SHOW_GRIDS, ops[n - 1u].kind);
}

void test_mqtt_display_cmd_brightness_reaches_panel_when_on(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"brightness\":2}"));
    TEST_ASSERT_EQUAL_UINT8(2u, fake_display_port_brightness());
}
