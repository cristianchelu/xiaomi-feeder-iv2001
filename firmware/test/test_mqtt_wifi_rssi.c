/* Tests: spec/30-processes/mqtt-protocol.md — wifi_rssi cadence */

#include "unity.h"

#include "fake_wifi_port.h"
#include "mqtt_outbox.h"
#include "mqtt_wifi_rssi.h"

static void setup(void)
{
    fake_wifi_port_reset();
    fake_wifi_port_set_sta_up(true, true);
    fake_wifi_port_set_rssi(-55);
    mqtt_outbox_reset();
    mqtt_outbox_set_accepting(true);
    mqtt_wifi_rssi_test_reset();
    mqtt_wifi_rssi_set_device_id("ddeeff");
}

void test_wifi_rssi_publishes_every_ten_minutes_or_on_change(void)
{
    setup();
    mqtt_wifi_rssi_poll(1000u, false);
    TEST_ASSERT_EQUAL_UINT(1u, mqtt_outbox_pending());
    fake_wifi_port_set_rssi(-58);                       /* small change */
    mqtt_wifi_rssi_poll(62000u, false);
    TEST_ASSERT_EQUAL_UINT(1u, mqtt_outbox_pending());
    fake_wifi_port_set_rssi(-61);                       /* >= 5 dB from -55 */
    mqtt_wifi_rssi_poll(123000u, false);
    TEST_ASSERT_EQUAL_UINT(2u, mqtt_outbox_pending());
    mqtt_wifi_rssi_poll(123000u + MQTT_WIFI_RSSI_PUBLISH_MS - 1000u, false);
    TEST_ASSERT_EQUAL_UINT(2u, mqtt_outbox_pending());
    mqtt_wifi_rssi_poll(123000u + MQTT_WIFI_RSSI_PUBLISH_MS + MQTT_WIFI_RSSI_PERIOD_MS, false);
    TEST_ASSERT_EQUAL_UINT(3u, mqtt_outbox_pending());
}
