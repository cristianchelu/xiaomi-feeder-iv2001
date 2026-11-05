/* Tests: spec/30-processes/mqtt-protocol.md § Reboot command */

#include "unity.h"

#include "app_cmd_dispatch.h"
#include "dispense.h"
#include "fake_system_port.h"
#include "feeder_runtime.h"
#include "reboot_request.h"

static void setup(void)
{
    dispense_test_reset();
    reboot_request_test_reset();
    fake_system_port_reset();
    feeder_runtime_set_ota_active(false);
}

void test_reboot_arm_does_not_reboot_immediately(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK, reboot_request_arm());
    TEST_ASSERT_TRUE(reboot_request_pending());
    TEST_ASSERT_EQUAL_UINT(0, fake_system_port_reboot_calls());
}

void test_reboot_fires_after_delay_from_first_tick(void)
{
    setup();
    (void)reboot_request_arm();
    reboot_request_poll(1000u);
    TEST_ASSERT_EQUAL_UINT(0, fake_system_port_reboot_calls());
    reboot_request_poll(1000u + REBOOT_REQUEST_DELAY_MS - 1u);
    TEST_ASSERT_EQUAL_UINT(0, fake_system_port_reboot_calls());
    reboot_request_poll(1000u + REBOOT_REQUEST_DELAY_MS);
    TEST_ASSERT_EQUAL_UINT(1, fake_system_port_reboot_calls());
    TEST_ASSERT_FALSE(reboot_request_pending());
}

void test_reboot_poll_idle_does_nothing(void)
{
    setup();
    reboot_request_poll(5000u);
    reboot_request_poll(9000u);
    TEST_ASSERT_EQUAL_UINT(0, fake_system_port_reboot_calls());
}

void test_reboot_second_arm_does_not_extend_deadline(void)
{
    setup();
    (void)reboot_request_arm();
    reboot_request_poll(100u);
    TEST_ASSERT_EQUAL(PORT_OK, reboot_request_arm());
    reboot_request_poll(100u + REBOOT_REQUEST_DELAY_MS);
    TEST_ASSERT_EQUAL_UINT(1, fake_system_port_reboot_calls());
}

void test_reboot_refused_while_ota_active(void)
{
    setup();
    feeder_runtime_set_ota_active(true);
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY, reboot_request_arm());
    TEST_ASSERT_FALSE(reboot_request_pending());
    feeder_runtime_set_ota_active(false);
}

void test_reboot_handles_tick_wraparound(void)
{
    setup();
    (void)reboot_request_arm();
    reboot_request_poll(0xFFFFFF00u);
    reboot_request_poll(0xFFFFFF00u + REBOOT_REQUEST_DELAY_MS);
    TEST_ASSERT_EQUAL_UINT(1, fake_system_port_reboot_calls());
}

void test_cmd_dispatch_reboot_arms_request(void)
{
    setup();
    TEST_ASSERT_EQUAL(PORT_OK,
                      app_cmd_dispatch(MQTT_ROUTE_CMD_REBOOT, "{}", 2u, "a4cf12"));
    TEST_ASSERT_TRUE(reboot_request_pending());
}

void test_cmd_dispatch_reboot_busy_during_ota(void)
{
    setup();
    feeder_runtime_set_ota_active(true);
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY,
                      app_cmd_dispatch(MQTT_ROUTE_CMD_REBOOT, "{}", 2u, "a4cf12"));
    TEST_ASSERT_FALSE(reboot_request_pending());
    feeder_runtime_set_ota_active(false);
}

void test_reboot_refused_while_dispense_active(void)
{
    setup();
    /* dispense_is_active() is true once a job is submitted. */
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(10u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY, reboot_request_arm());
    TEST_ASSERT_FALSE(reboot_request_pending());
    dispense_test_reset();
}
