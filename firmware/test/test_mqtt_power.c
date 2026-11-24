/* Tests: spec/30-processes/mqtt-protocol.md § Power settings */

#include <string.h>

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "fake_config_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "mqtt_outbox.h"
#include "mqtt_power.h"
#include "mqtt_route.h"

#define TEST_DEVICE_ID "ddeeff"

static void setup(void)
{
    fake_config_port_reset();
    fake_mqtt_port_reset();
    fake_time_reset();
    mqtt_outbox_reset();
    mqtt_power_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    mqtt_power_set_device_id(TEST_DEVICE_ID);
}

static port_err_t cmd(const char *s)
{
    return app_cmd_dispatch(MQTT_ROUTE_CMD_POWER, s, strlen(s), TEST_DEVICE_ID);
}

void test_mqtt_power_default_is_on(void)
{
    setup();
    TEST_ASSERT_EQUAL(BATTERY_WIFI_ON, mqtt_power_battery_wifi_get());
}

void test_mqtt_power_cmd_persists_and_publishes(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"battery_wifi\":\"scheduled_only\"}"));
    TEST_ASSERT_EQUAL(BATTERY_WIFI_SCHEDULED_ONLY, mqtt_power_battery_wifi_get());
    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(fake_mqtt_port_get())) {
            fake_time_advance_ms(101u);
        }
    }
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/power", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("{\"battery_wifi\":\"scheduled_only\"}", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}

void test_mqtt_power_cmd_rejects_bad_values(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"battery_wifi\":\"sometimes\"}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd(""));
    TEST_ASSERT_EQUAL(BATTERY_WIFI_ON, mqtt_power_battery_wifi_get());
}
