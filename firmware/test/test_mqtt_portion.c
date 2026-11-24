/* Tests: spec/30-processes/mqtt-protocol.md § Portion size */

#include <string.h>

#include "unity.h"

#include "app.h"
#include "app_cmd_dispatch.h"
#include "dispense.h"
#include "fake_config_port.h"
#include "fake_motor_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "feeder_runtime.h"
#include "motor_port.h"
#include "mqtt_outbox.h"
#include "mqtt_portion.h"
#include "mqtt_route.h"
#include "portion_estimate.h"

#define TEST_DEVICE_ID "ddeeff"

extern void fake_app_event_q_reset(void);
extern void motor_port_host_reset(void);

static void setup(void)
{
    fake_config_port_reset();
    fake_mqtt_port_reset();
    fake_time_reset();
    mqtt_outbox_reset();
    motor_port_host_reset();
    fake_motor_port_reset();
    fake_app_event_q_reset();
    dispense_test_reset();
    portion_estimate_test_reset();
    mqtt_portion_test_reset();
    mqtt_outbox_set_accepting(true);
    fake_mqtt_port_get()->connect(NULL);
    mqtt_portion_set_device_id(TEST_DEVICE_ID);
}

static void drain(void)
{
    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(fake_mqtt_port_get())) {
            fake_time_advance_ms(101u);
        }
    }
}

static port_err_t cmd(const char *s)
{
    return app_cmd_dispatch(MQTT_ROUTE_CMD_PORTION_CAL, s, strlen(s), TEST_DEVICE_ID);
}

void test_mqtt_portion_snapshot_default(void)
{
    char buf[64];

    setup();
    TEST_ASSERT_TRUE(mqtt_portion_format_snapshot(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"g\":\"10.0\",\"status\":\"default\"}", buf);
}

void test_mqtt_portion_calibrate_starts_three_portion_job(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{}"));
    TEST_ASSERT_TRUE(app_step());
    TEST_ASSERT_EQUAL(PORTION_CAL_PORTIONS, fake_motor_port_last_pulse_target());
}

void test_portion_estimate_calibrate_sets_value_and_status(void)
{
    char buf[64];

    setup();
    TEST_ASSERT_TRUE(portion_estimate_calibrate(255, 3u));   /* field: 25.5 g */
    TEST_ASSERT_EQUAL_INT32(85, portion_estimate_dg());
    mqtt_portion_sync();
    drain();
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/portion", fake_mqtt_port_state()->last_publish_topic);
    TEST_ASSERT_TRUE(mqtt_portion_format_snapshot(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"g\":\"8.5\",\"status\":\"ok\"}", buf);
}

void test_portion_estimate_calibrate_rejects_nothing_delivered(void)
{
    setup();
    TEST_ASSERT_FALSE(portion_estimate_calibrate(0, 3u));
    TEST_ASSERT_FALSE(portion_estimate_calibrate(30, 3u));     /* 1 g per portion */
    TEST_ASSERT_EQUAL(PORTION_CAL_FAILED, portion_estimate_status());
    TEST_ASSERT_EQUAL_INT32(100, portion_estimate_dg());
}

void test_mqtt_portion_reset_restores_default(void)
{
    setup();
    TEST_ASSERT_TRUE(portion_estimate_calibrate(255, 3u));
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"action\":\"reset\"}"));
    TEST_ASSERT_EQUAL_INT32(100, portion_estimate_dg());
    TEST_ASSERT_EQUAL(PORTION_CAL_NONE, portion_estimate_status());
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"action\":\"bogus\"}"));
}
