/* Tests: spec/30-processes/mqtt-protocol.md § Eaten today */

#include <string.h>

#include "unity.h"

#include "eaten_today.h"
#include "fake_config_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "mqtt_eaten_today.h"
#include "mqtt_outbox.h"

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
    eaten_today_test_reset();
    eaten_today_init(20261105u);
    mqtt_outbox_reset();
    mqtt_eaten_today_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    mqtt_eaten_today_set_device_id(TEST_DEVICE_ID);
}

static void eat(weight_dg_t from_dg, weight_dg_t to_dg)
{
    for (int i = 0; i < 8; i++) {
        eaten_today_on_idle_sample(from_dg, true, 20261105u);
    }
    for (int i = 0; i < 8; i++) {
        eaten_today_on_idle_sample(to_dg, true, 20261105u);
    }
}

void test_mqtt_eaten_today_format_has_one_decimal(void)
{
    char buf[16];

    setup();
    TEST_ASSERT_TRUE(mqtt_eaten_today_format(0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("0.0", buf);
    TEST_ASSERT_TRUE(mqtt_eaten_today_format(853, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("85.3", buf);
    TEST_ASSERT_FALSE(mqtt_eaten_today_format(1, buf, 2u));
}

void test_mqtt_eaten_today_connect_snapshot_publishes_retained(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    mqtt_eaten_today_connect_snapshot();
    drain_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/eaten_today", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("0.0", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}

void test_mqtt_eaten_today_sync_publishes_only_on_change(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    mqtt_eaten_today_connect_snapshot();
    drain_outbox();
    mqtt_eaten_today_sync();
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());

    eat(500, 420);
    mqtt_eaten_today_sync();
    drain_outbox();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("8.0", mqtt->last_publish_payload);

    mqtt_eaten_today_sync();
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
}
