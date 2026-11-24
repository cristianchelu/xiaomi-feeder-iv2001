/* Tests: spec/30-processes/mqtt-protocol.md § Child lock */

#include <string.h>

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "fake_config_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "feed_config.h"
#include "mqtt_child_lock.h"
#include "mqtt_outbox.h"
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

static void setup_child_lock_mqtt(void)
{
    fake_config_port_reset();
    fake_mqtt_port_reset();
    fake_time_reset();
    mqtt_outbox_reset();
    mqtt_child_lock_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    mqtt_child_lock_set_device_id(TEST_DEVICE_ID);
}

void test_mqtt_child_lock_connect_snapshot_publishes_off_retained(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_child_lock_mqtt();
    mqtt_child_lock_connect_snapshot();
    drain_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/child_lock", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("OFF", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}

void test_mqtt_child_lock_connect_snapshot_publishes_on_when_locked(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_child_lock_mqtt();
    TEST_ASSERT_TRUE(feed_config_child_lock_set(true));
    mqtt_child_lock_connect_snapshot();
    drain_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("ON", mqtt->last_publish_payload);
}

void test_mqtt_child_lock_apply_persists_and_publishes_on_change(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_child_lock_apply(true));
    drain_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_TRUE(feed_config_child_lock_is_active());
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/child_lock", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("ON", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}

void test_mqtt_child_lock_apply_unchanged_skips_publish(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_child_lock_apply(false));
    drain_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->publish_calls);
}

void test_mqtt_child_lock_handle_plain_text_on_off(void)
{
    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_child_lock_handle("ON", strlen("ON")));
    TEST_ASSERT_TRUE(feed_config_child_lock_is_active());
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_child_lock_handle("OFF", strlen("OFF")));
    TEST_ASSERT_FALSE(feed_config_child_lock_is_active());
}

void test_mqtt_child_lock_handle_ignores_surrounding_whitespace(void)
{
    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_child_lock_handle(" ON\r\n", strlen(" ON\r\n")));
    TEST_ASSERT_TRUE(feed_config_child_lock_is_active());
}

void test_mqtt_child_lock_handle_rejects_invalid_payload(void)
{
    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_child_lock_handle("on", strlen("on")));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_child_lock_handle("true", strlen("true")));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_child_lock_handle("", 0u));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_child_lock_handle(NULL, 2u));
    TEST_ASSERT_FALSE(feed_config_child_lock_is_active());
}

void test_mqtt_child_lock_dispatch_route_applies_command(void)
{
    setup_child_lock_mqtt();
    TEST_ASSERT_EQUAL(MQTT_ROUTE_CMD_CHILD_LOCK,
                      mqtt_route_classify("petfeeder/ddeeff/cmd/child_lock", TEST_DEVICE_ID));
    TEST_ASSERT_EQUAL(PORT_OK,
                      app_cmd_dispatch(MQTT_ROUTE_CMD_CHILD_LOCK, "ON", 2u, TEST_DEVICE_ID));
    TEST_ASSERT_TRUE(feed_config_child_lock_is_active());
}

void test_mqtt_child_lock_label_names_route(void)
{
    TEST_ASSERT_EQUAL_STRING("child_lock", mqtt_route_label(MQTT_ROUTE_CMD_CHILD_LOCK));
}
