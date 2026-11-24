/* Tests: spec/30-processes/ota-flow.md § One-button install */

#include <string.h>

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "fake_config_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "mqtt_outbox.h"
#include "mqtt_route.h"
#include "ota_base.h"

#define TEST_DEVICE_ID "ddeeff"

static void setup(void)
{
    fake_config_port_reset();
    fake_mqtt_port_reset();
    mqtt_outbox_reset();
    ota_base_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    ota_base_set_device_id(TEST_DEVICE_ID);
}

static port_err_t cmd(const char *s)
{
    return app_cmd_dispatch(MQTT_ROUTE_CMD_OTA_BASE, s, strlen(s), TEST_DEVICE_ID);
}

void test_ota_base_build_url_picks_inactive_bank_file(void)
{
    char url[160];

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("http://10.20.10.5:8123/local/feeder"));
    TEST_ASSERT_TRUE(ota_base_build_url('a', url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://10.20.10.5:8123/local/feeder/petfeeder_a.bin", url);
    TEST_ASSERT_TRUE(ota_base_build_url('b', url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("http://10.20.10.5:8123/local/feeder/petfeeder_b.bin", url);
}

void test_ota_base_trailing_slash_trimmed_and_persisted(void)
{
    char base[96];

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("http://host/dir/"));
    TEST_ASSERT_TRUE(ota_base_get(base, sizeof(base)));
    TEST_ASSERT_EQUAL_STRING("http://host/dir", base);
}

void test_ota_base_rejects_non_http_and_too_long(void)
{
    char big[200];

    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("ftp://host/x"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd(""));
    memset(big, 'a', sizeof(big) - 1u);
    memcpy(big, "http://", 7u);
    big[sizeof(big) - 1u] = '\0';
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd(big));
    TEST_ASSERT_FALSE(ota_base_build_url('a', big, sizeof(big)));
}

void test_ota_base_publishes_retained_state(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("http://host/d"));
    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(fake_mqtt_port_get())) {
            fake_time_advance_ms(101u);
        }
    }
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/ota/base", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("http://host/d", mqtt->last_publish_payload);
    TEST_ASSERT_TRUE(mqtt->last_publish_retain);
}
