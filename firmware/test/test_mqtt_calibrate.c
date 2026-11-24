/* Tests: spec/30-processes/mqtt-protocol.md § Calibrate command */

#include <string.h>

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "dispense.h"
#include "fake_weight_port.h"
#include "fake_motor_port.h"
#include "feeder_runtime.h"
#include "motor_port_provider_host.h"
#include "mqtt_route.h"

extern void fake_app_event_q_reset(void);

static void setup(void)
{
    fake_weight_port_reset();
    motor_port_host_reset();
    fake_motor_port_reset();
    fake_app_event_q_reset();
    dispense_test_reset();
    feeder_runtime_set_ota_active(false);
}

static port_err_t cmd(const char *json)
{
    return app_cmd_dispatch(MQTT_ROUTE_CMD_CALIBRATE, json, strlen(json), "ddeeff");
}

static size_t ops_of(fake_weight_op_kind_t kind)
{
    size_t n = 0u;
    size_t total = 0u;
    const fake_weight_op_t *ops = fake_weight_port_ops(&total);

    for (size_t i = 0u; i < total; i++) {
        if (ops[i].kind == kind) {
            n++;
        }
    }
    return n;
}

void test_mqtt_calibrate_zero_captures_zero(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"action\":\"zero\"}"));
    TEST_ASSERT_EQUAL_UINT(1, ops_of(FAKE_WEIGHT_OP_CAL_ZERO));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_SPAN));
}

void test_mqtt_calibrate_span_captures_span(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cmd("{\"action\":\"span\"}"));
    TEST_ASSERT_EQUAL_UINT(1, ops_of(FAKE_WEIGHT_OP_CAL_SPAN));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_ZERO));
}

void test_mqtt_calibrate_rejects_bad_payloads_without_capture(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd(""));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("zero"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"action\":\"tare\"}"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, cmd("{\"action\":\"\"}"));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_ZERO));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_SPAN));
}

void test_mqtt_calibrate_busy_during_ota(void)
{
    setup();
    feeder_runtime_set_ota_active(true);
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY, cmd("{\"action\":\"zero\"}"));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_ZERO));
    feeder_runtime_set_ota_active(false);
}

void test_mqtt_calibrate_busy_during_dispense(void)
{
    setup();
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(10u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY, cmd("{\"action\":\"span\"}"));
    TEST_ASSERT_EQUAL_UINT(0, ops_of(FAKE_WEIGHT_OP_CAL_SPAN));
    dispense_test_reset();
}
