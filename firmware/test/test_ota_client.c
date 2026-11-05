/* Tests: spec/30-processes/ota-flow.md, spec/30-processes/mqtt-protocol.md */

#include <string.h>

#include "unity.h"

#include "dispense.h"
#include "fake_mqtt_port.h"
#include "fake_motor_port.h"
#include "motor_port_provider_host.h"
#include "fake_ota_port.h"
#include "fake_time.h"
#include "fake_boot_bank.h"
#include "mqtt_outbox.h"
#include "fake_config_port.h"
#include "ota_base.h"
#include "ota_client.h"

#define TEST_DEVICE_ID "ddeeff"

extern void fake_app_event_q_reset(void);

static void drain_ota_outbox(void)
{
    const mqtt_port_t *mqtt = fake_mqtt_port_get();

    while (mqtt_outbox_pending() > 0) {
        if (!mqtt_outbox_drain_one(mqtt)) {
            fake_time_advance_ms(101u);
            (void)mqtt_outbox_drain_one(mqtt);
        }
    }
}

static void setup_ota_client_connected(void)
{
    motor_port_host_reset();
    fake_motor_port_reset();
    fake_app_event_q_reset();
    dispense_test_reset();
    fake_mqtt_port_reset();
    fake_ota_port_reset();
    mqtt_outbox_reset();

    fake_mqtt_port_get()->connect(NULL);
    ota_client_set_device_id(TEST_DEVICE_ID);
    ota_client_start();
}

void test_ota_cmd_valid_starts_download(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"url\":\"http://10.0.0.5/fw.bin\"}";

    setup_ota_client_connected();

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    ota = fake_ota_port_state();

    TEST_ASSERT_EQUAL_UINT(1, ota->start_calls);
    TEST_ASSERT_EQUAL_STRING("http://10.0.0.5/fw.bin", ota->last_url);
    TEST_ASSERT_FALSE(ota->last_has_sha512);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/ota/status", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"downloading\",\"pct\":0,\"error\":\"\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_cmd_already_in_progress(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"url\":\"http://10.0.0.5/fw.bin\"}";

    setup_ota_client_connected();
    fake_ota_port_set_start_result(PORT_ERR_BUSY);

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    ota = fake_ota_port_state();

    TEST_ASSERT_EQUAL_UINT(1, ota->start_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"already_in_progress\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_cmd_invalid_url(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"url\":\"file:///tmp/fw.bin\"}";

    setup_ota_client_connected();

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    ota = fake_ota_port_state();

    TEST_ASSERT_EQUAL_UINT(0, ota->start_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"invalid_url\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_cmd_wrong_topic_ignored(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/dispense";
    const char *payload = "{\"url\":\"http://10.0.0.5/fw.bin\"}";

    setup_ota_client_connected();

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    ota = fake_ota_port_state();

    TEST_ASSERT_EQUAL_UINT(0, ota->start_calls);
    TEST_ASSERT_EQUAL_UINT(0, mqtt->publish_calls);
}

void test_ota_on_mqtt_connected_publishes_idle(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_ota_client_connected();

    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/ota/status", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"idle\",\"pct\":0,\"error\":\"\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_on_mqtt_connected_includes_bank_b(void)
{
    const fake_mqtt_port_state_t *mqtt;

    setup_ota_client_connected();
    fake_boot_bank_set_active(BOOT_BANK_B);

    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"idle\",\"pct\":0,\"error\":\"\",\"bank\":\"B\"}",
                             mqtt->last_publish_payload);
    fake_boot_bank_reset();
}

void test_ota_progress_callback_publishes_status(void)
{
    const fake_mqtt_port_state_t *mqtt;
    ota_progress_t progress = {
        .status = OTA_STATUS_DOWNLOADING,
        .pct = 50,
        .error = NULL,
    };

    setup_ota_client_connected();

    fake_ota_port_emit_progress(&progress);
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"downloading\",\"pct\":50,\"error\":\"\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

/* spec/30-processes/mqtt-protocol.md § OTA status — remembered status */

void test_ota_progress_not_enqueued_while_broker_session_closed(void)
{
    ota_progress_t progress = { .status = OTA_STATUS_DOWNLOADING, .pct = 40, .error = NULL };

    setup_ota_client_connected();
    mqtt_outbox_set_accepting(false);

    fake_ota_port_emit_progress(&progress);

    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
    TEST_ASSERT_EQUAL_UINT(0, fake_mqtt_port_state()->publish_calls);
    mqtt_outbox_set_accepting(true);
}

void test_ota_error_status_survives_reconnect(void)
{
    const fake_mqtt_port_state_t *mqtt;
    ota_progress_t progress = { .status = OTA_STATUS_ERROR, .pct = 0, .error = "verify_failed" };

    setup_ota_client_connected();

    fake_ota_port_emit_progress(&progress);
    drain_ota_outbox();
    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(2, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"verify_failed\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_error_reported_while_closed_is_published_on_connect(void)
{
    const fake_mqtt_port_state_t *mqtt;
    ota_progress_t progress = { .status = OTA_STATUS_ERROR, .pct = 0, .error = "download_failed" };

    setup_ota_client_connected();
    mqtt_outbox_set_accepting(false);
    fake_ota_port_emit_progress(&progress);
    mqtt_outbox_set_accepting(true);

    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"download_failed\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

void test_ota_new_cmd_after_error_publishes_downloading(void)
{
    const fake_mqtt_port_state_t *mqtt;
    ota_progress_t progress = { .status = OTA_STATUS_ERROR, .pct = 0, .error = "verify_failed" };
    const char *payload = "{\"url\":\"http://10.0.0.5/fw.bin\"}";

    setup_ota_client_connected();
    fake_ota_port_emit_progress(&progress);
    drain_ota_outbox();

    ota_client_on_mqtt_message("petfeeder/ddeeff/cmd/ota", payload, strlen(payload));
    drain_ota_outbox();
    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"downloading\",\"pct\":0,\"error\":\"\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
}

/* spec/30-processes/ota-flow.md § Slot health — Application: rollback report */

void test_ota_rollback_mark_reports_error_on_connect_and_clears(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_mqtt_port_reset();
    fake_ota_port_reset();
    mqtt_outbox_reset();
    fake_boot_bank_reset();
    fake_boot_bank_set_rolled_back(true);
    fake_mqtt_port_get()->connect(NULL);
    ota_client_set_device_id(TEST_DEVICE_ID);
    ota_client_start();

    TEST_ASSERT_FALSE(fake_boot_bank_rolled_back());

    ota_client_on_mqtt_connected();
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"rolled_back\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
    fake_boot_bank_reset();
}

void test_ota_cmd_rejected_while_dispense_active(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"url\":\"http://10.0.0.5/fw.bin\"}";

    setup_ota_client_connected();
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(10u, DISPENSE_SOURCE_MQTT));

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    mqtt = fake_mqtt_port_state();
    ota = fake_ota_port_state();
    TEST_ASSERT_EQUAL_UINT(0, ota->start_calls);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"error\",\"pct\":0,\"error\":\"busy_dispense\",\"bank\":\"A\"}",
                             mqtt->last_publish_payload);
    dispense_test_reset();
}

void test_ota_install_uses_base_url_and_inactive_bank(void)
{
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"install\":true}";
    const char *base = "http://10.20.10.5:8123/local/feeder";

    setup_ota_client_connected();
    TEST_ASSERT_EQUAL(PORT_OK, ota_base_handle(base, strlen(base)));

    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    ota = fake_ota_port_state();
    TEST_ASSERT_EQUAL_UINT(1, ota->start_calls);
    TEST_ASSERT_EQUAL_STRING("http://10.20.10.5:8123/local/feeder/petfeeder_b.bin", ota->last_url);
}

void test_ota_install_without_base_reports_error(void)
{
    const fake_mqtt_port_state_t *mqtt;
    const fake_ota_port_state_t *ota;
    const char *topic = "petfeeder/ddeeff/cmd/ota";
    const char *payload = "{\"install\":true}";

    setup_ota_client_connected();
    fake_config_port_reset();
    ota_client_on_mqtt_message(topic, payload, strlen(payload));
    drain_ota_outbox();

    ota = fake_ota_port_state();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, ota->start_calls);
    TEST_ASSERT_NOT_NULL(strstr(mqtt->last_publish_payload, "no_base_url"));
}
