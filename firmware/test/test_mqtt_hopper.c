/* Tests: spec/30-processes/mqtt-protocol.md § hopper */

#include <string.h>

#include "unity.h"

#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "hopper_level.h"
#include "mqtt_hopper.h"
#include "mqtt_outbox.h"

#define TEST_DEVICE_ID "ddeeff"

static void drain_hopper_outbox(void)
{
    const mqtt_port_t *mqtt = fake_mqtt_port_get();

    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(mqtt)) {
            fake_time_advance_ms(101u);
            (void)mqtt_outbox_drain_one(mqtt);
        }
    }
}

static void setup_mqtt_hopper(void)
{
    fake_mqtt_port_reset();
    mqtt_outbox_reset();
    mqtt_hopper_test_reset();
    fake_mqtt_port_get()->connect(NULL);
    mqtt_hopper_set_device_id(TEST_DEVICE_ID);
}

void test_mqtt_hopper_sync_publishes_normal(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/hopper", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("normal", mqtt->last_publish_payload);
}

void test_mqtt_hopper_sync_publishes_low(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_LOW);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("low", mqtt->last_publish_payload);
}

void test_mqtt_hopper_sync_publishes_empty(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_EMPTY);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("empty", mqtt->last_publish_payload);
}

void test_mqtt_hopper_sync_edge_detects_unchanged_value(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_LOW);
    drain_hopper_outbox();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_LOW);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
}

void test_mqtt_hopper_sync_publishes_on_change(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_EMPTY);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(2, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("empty", mqtt->last_publish_payload);
}

void test_mqtt_hopper_connect_snapshot_publishes_once(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_connect_snapshot(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("normal", mqtt->last_publish_payload);

    mqtt_hopper_connect_snapshot(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();
    TEST_ASSERT_EQUAL_UINT(2, mqtt->publish_calls);
}

void test_mqtt_hopper_on_mqtt_connected_snapshots_known_value(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_LOW);
    drain_hopper_outbox();

    fake_mqtt_port_reset();
    fake_mqtt_port_get()->connect(NULL);
    mqtt_outbox_reset();

    mqtt_hopper_on_mqtt_connected();
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("low", mqtt->last_publish_payload);
}

void test_mqtt_hopper_on_mqtt_connected_skips_when_unknown(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_on_mqtt_connected();
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->publish_calls);
}

void test_mqtt_hopper_on_outbox_reset_allows_republish(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();

    mqtt_hopper_on_outbox_reset();
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_NORMAL);
    drain_hopper_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(2, mqtt->publish_calls);
}

void test_mqtt_hopper_ir_publishes_on_change_only(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_mqtt_hopper();
    mqtt_hopper_ir_sync(-1);
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());

    mqtt_hopper_ir_sync(1);
    drain_hopper_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/hopper_ir", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("blocked", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);

    mqtt_hopper_ir_sync(1);
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());

    mqtt_hopper_ir_sync(0);
    drain_hopper_outbox();
    TEST_ASSERT_EQUAL_STRING("clear", fake_mqtt_port_state()->last_publish_payload);
}

void test_mqtt_hopper_ir_connect_snapshot_republishes(void)
{
    setup_mqtt_hopper();
    mqtt_hopper_ir_sync(0);
    drain_hopper_outbox();
    mqtt_hopper_ir_connect_snapshot(0);
    TEST_ASSERT_EQUAL_UINT(1, mqtt_outbox_pending());
    drain_hopper_outbox();
    mqtt_hopper_ir_connect_snapshot(-1);
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
}
