/* Tests: spec/30-processes/mqtt-protocol.md § Dispense progress */

#include <string.h>

#include "unity.h"

#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "mqtt_dispense_progress.h"
#include "mqtt_outbox.h"

static void progress_setup(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    mqtt_outbox_reset();
    mqtt_outbox_set_accepting(true);
    mqtt_dispense_progress_test_reset();
    mqtt_dispense_progress_set_device_id("ddeeff");
}

void test_dispense_progress_formats_running_state(void)
{
    char buf[96];

    progress_setup();
    mqtt_dispense_progress_update("running", 2u, 5u, 173);
    TEST_ASSERT_TRUE(mqtt_dispense_progress_format(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"running\",\"portions\":2,\"of\":5,\"g\":\"17.3\"}", buf);
    TEST_ASSERT_EQUAL_UINT(1u, mqtt_outbox_pending());
}

void test_dispense_progress_skips_unchanged_payload(void)
{
    progress_setup();
    mqtt_dispense_progress_update("running", 1u, 3u, 90);
    mqtt_dispense_progress_update("running", 1u, 3u, 90);
    TEST_ASSERT_EQUAL_UINT(1u, mqtt_outbox_pending());
    mqtt_dispense_progress_update("weighing", 3u, 3u, 270);
    TEST_ASSERT_EQUAL_UINT(2u, mqtt_outbox_pending());
}

void test_dispense_progress_yields_when_outbox_busy(void)
{
    unsigned i;

    progress_setup();
    for (i = 0u; i < 8u; i++) {
        TEST_ASSERT_TRUE(mqtt_outbox_enqueue("t", "x", 1u, 1, false));
    }
    mqtt_dispense_progress_update("running", 1u, 3u, 90);
    TEST_ASSERT_EQUAL_UINT(8u, mqtt_outbox_pending());
}
