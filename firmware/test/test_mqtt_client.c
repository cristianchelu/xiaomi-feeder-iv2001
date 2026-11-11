/* Tests: spec/30-processes/uart-console.md (MQTT connect/disconnect), mqtt-protocol.md */

#include "unity.h"

#include "config_keys.h"
#include "fake_config_port.h"
#include "fake_mqtt_port.h"
#include "fake_time.h"
#include "fake_adc_port.h"
#include "fake_power_source_port.h"
#include "mqtt_ha_discovery.h"
#include "mqtt_outbox.h"
#include "fake_wifi_port.h"
#include <string.h>

#include "app.h"
#include "app_event.h"
#include "app_log.h"
#include "display_presentation.h"
#include "fake_display_port.h"
#include "mqtt_client.h"
#include "mqtt_client_test.h"
#include "mqtt_cred.h"

static void seed_broker_config(void)
{
    const config_port_t *cfg = fake_config_port_get();

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_cred_save_host(cfg, "broker.local"));
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_cred_save_port(cfg, 1883));
}

static void post_display_tick(uint32_t now_ms)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = EVT_DISPLAY_TICK;
    ev.u.display_tick.now_ms = now_ms;
    TEST_ASSERT_TRUE(app_event_post(&ev));
}

static void setup_wifi_up(void)
{
    fake_wifi_port_reset();
    fake_wifi_port_set_sta_up(true, true);
    fake_adc_port_reset();
    fake_power_source_port_reset();
    app_test_reset();
    mqtt_client_notify_wifi_ready();
}

static char s_log_capture[512];
static size_t s_log_capture_len;

static void mqtt_log_sink(const char *buf, size_t len, void *ctx)
{
    size_t room;

    (void)ctx;

    if (len == 0u) {
        return;
    }

    room = sizeof(s_log_capture) - s_log_capture_len;
    if (len > room) {
        len = room;
    }

    memcpy(s_log_capture + s_log_capture_len, buf, len);
    s_log_capture_len += len;
}

static void mqtt_log_capture_begin(void)
{
    app_log_test_reset();
    s_log_capture_len = 0;
    memset(s_log_capture, 0, sizeof(s_log_capture));
    app_log_set_sink(mqtt_log_sink, NULL);
}

static void mqtt_log_capture_end(void)
{
    app_log_clear_sink();
}

void test_client_idle_when_not_armed(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    mqtt_client_step();
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->connect_calls);
}

void test_request_connect_posts_connecting_session(void)
{
    uint8_t grids[TM1637_GRID_COUNT];

    app_test_reset();
    fake_display_port_reset();
    display_presentation_reset();
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    app_step();
    post_display_tick(0u);
    app_step();

    fake_display_port_last_grids(grids);
    TEST_ASSERT_EQUAL_HEX8(0x01u, grids[4]);
}

void test_request_connect_requires_wifi(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();

    TEST_ASSERT_FALSE(mqtt_client_request_connect());
    TEST_ASSERT_FALSE(mqtt_client_connect_in_progress());
}

/* Drain the outbox while display ticks pump paced HA discovery.
 * Returns the number of HA discovery configs published. */
static unsigned drain_with_discovery(const fake_mqtt_port_state_t *mqtt, unsigned max_iter)
{
    unsigned discovery = 0;
    unsigned last_calls = mqtt->publish_calls;
    unsigned i;

    for (i = 0; i < max_iter; i++) {
        fake_time_advance_ms(101u);
        mqtt_client_step();
        if (mqtt->publish_calls != last_calls) {
            last_calls = mqtt->publish_calls;
            if (strncmp(mqtt->last_publish_topic, "homeassistant/", 14) == 0) {
                discovery++;
            }
        }
        post_display_tick(i);
        app_step();
        if (mqtt_outbox_pending() == 0u && mqtt_ha_discovery_remaining() == 0u) {
            break;
        }
    }
    return discovery;
}

void test_connect_subscribes_and_publishes_online(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();
    app_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->subscribe_calls);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/cmd/#", mqtt->last_subscribe_topic);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/connection", mqtt->last_publish_topic);
    TEST_ASSERT_EQUAL_STRING("online", mqtt->last_publish_payload);
    /* Discovery is paced: the connect burst leaves room in the ring. */
    TEST_ASSERT_EQUAL_UINT(17, mqtt_outbox_pending());
    TEST_ASSERT_EQUAL_UINT(mqtt_ha_discovery_entity_count(), mqtt_ha_discovery_remaining());
    TEST_ASSERT_TRUE(mqtt->connected);

    mqtt_client_step();
    TEST_ASSERT_EQUAL_UINT(4, mqtt->publish_calls);
    TEST_ASSERT_EQUAL_STRING("petfeeder/ddeeff/schedule/next", mqtt->last_publish_topic);

    TEST_ASSERT_EQUAL_UINT(mqtt_ha_discovery_entity_count(), drain_with_discovery(mqtt, 400u));
    TEST_ASSERT_EQUAL_UINT(20u + mqtt_ha_discovery_entity_count(), mqtt->publish_calls);
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
    TEST_ASSERT_EQUAL_UINT(0, mqtt_ha_discovery_remaining());
}

void test_connected_session_survives_yield_steps(void)
{
    const fake_mqtt_port_state_t *mqtt;
    int i;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    for (i = 0; i < 20; i++) {
        mqtt_client_step();
    }

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);
    TEST_ASSERT_EQUAL_UINT(0, mqtt->disconnect_calls);
    TEST_ASSERT_TRUE(mqtt->connected);
}

void test_connected_step_drains_enqueued_item(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();
    app_step();

    TEST_ASSERT_EQUAL_UINT(17, mqtt_outbox_pending());
    mqtt = fake_mqtt_port_state();
    mqtt_client_step();
    (void)drain_with_discovery(mqtt, 400u);
    TEST_ASSERT_EQUAL_UINT(20u + mqtt_ha_discovery_entity_count(), mqtt->publish_calls);
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());
}

void test_stop_disarms_and_disconnects(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    mqtt_client_stop();
    mqtt_client_step();
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);
    TEST_ASSERT_FALSE(mqtt->connected);
}

void test_connect_failure_honors_backoff(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    fake_mqtt_port_set_fail_next_connect(true);

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);

    mqtt_client_step();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);

    fake_time_advance_ms(2000);
    mqtt_client_step();
    TEST_ASSERT_EQUAL_UINT(2, mqtt->connect_calls);
}

void test_bootstrap_without_autoconnect_flag_does_not_connect(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    mqtt_client_step();
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->connect_calls);
}

void test_mqtt_stop_clears_connect_in_progress(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    TEST_ASSERT_TRUE(mqtt_client_connect_in_progress());

    mqtt_client_stop();
    TEST_ASSERT_FALSE(mqtt_client_connect_in_progress());

    mqtt_client_step();
    TEST_ASSERT_EQUAL_UINT(0, fake_mqtt_port_state()->connect_calls);
}

void test_mqtt_connect_posts_connected_session_indicator(void)
{
    uint8_t grids[TM1637_GRID_COUNT];

    app_test_reset();
    fake_display_port_reset();
    display_presentation_reset();
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();
    app_step();
    post_display_tick(0u);
    app_step();

    fake_display_port_last_grids(grids);
    TEST_ASSERT_EQUAL_HEX8(0x02u, grids[4]);
    TEST_ASSERT_FALSE(mqtt_client_connect_in_progress());
}

void test_stored_host_autoconnects_on_wifi_ready(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_start();
    setup_wifi_up();

    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->connect_calls);
}

/* Regression: OTA must not spawn MQTT reconnect while HTTP download runs. */
void test_suspend_for_ota_blocks_pending_connect(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_suspend_for_ota();
    mqtt_client_step();
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(0, mqtt->connect_calls);
    TEST_ASSERT_FALSE(mqtt_client_connect_in_progress());

    mqtt_client_resume_after_ota();
}

void test_suspend_for_ota_pauses_outbox_enqueue(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    mqtt_outbox_reset();
    mqtt_outbox_set_accepting(true);
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();
    mqtt_client_suspend_for_ota();
    mqtt_client_step();

    TEST_ASSERT_FALSE(mqtt_outbox_is_accepting());
    TEST_ASSERT_FALSE(mqtt_outbox_enqueue("petfeeder/ddeeff/state", "{}", 2, 1, true));
    TEST_ASSERT_EQUAL_UINT(0, mqtt_outbox_pending());

    mqtt_client_resume_after_ota();
    TEST_ASSERT_TRUE(mqtt_outbox_is_accepting());
}

void test_connect_logs_connecting_and_connected_milestones(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    mqtt_log_capture_begin();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    mqtt_log_capture_end();
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "[mqtt]"));
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "connecting to broker.local:1883"));
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "connected"));
}

void test_connect_failure_logs_once_per_burst(void)
{
    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    fake_mqtt_port_set_fail_next_connect(true);
    mqtt_log_capture_begin();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    mqtt_log_capture_end();
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "connecting to broker.local:1883"));
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "connect failed"));
    TEST_ASSERT_NULL(strstr(s_log_capture, "connected"));

    mqtt_log_capture_begin();
    mqtt_client_step();
    mqtt_log_capture_end();
    TEST_ASSERT_NULL(strstr(s_log_capture, "connect failed"));
}

void test_suspend_for_ota_disconnects_without_reconnect(void)
{
    const fake_mqtt_port_state_t *mqtt;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();

    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();

    mqtt_client_suspend_for_ota();
    mqtt_client_step();
    mqtt_client_step();

    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);
    TEST_ASSERT_EQUAL_UINT(1, mqtt->disconnect_calls);
    TEST_ASSERT_FALSE(mqtt->connected);

    mqtt_client_resume_after_ota();
}

/* Device log: "publish failed topic=…/wifi_rssi, schedule/state, schedule/next"
 * every 250 ms for an hour; the session stayed "connected" and never
 * reconnected. Consecutive publish failures must drop and redo the session. */
void test_publish_failures_force_reconnect(void)
{
    const fake_mqtt_port_state_t *mqtt;
    unsigned i;

    fake_time_reset();
    fake_mqtt_port_reset();
    seed_broker_config();
    mqtt_client_test_bootstrap();
    setup_wifi_up();
    TEST_ASSERT_TRUE(mqtt_client_request_connect());
    mqtt_client_step();
    app_step();
    mqtt = fake_mqtt_port_state();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->connect_calls);

    fake_mqtt_port_set_fail_publish(true);
    mqtt_log_capture_begin();
    for (i = 0u; i < 20u && mqtt->disconnect_calls == 0u; i++) {
        fake_time_advance_ms(260u);
        mqtt_client_step();
    }
    mqtt_log_capture_end();
    TEST_ASSERT_EQUAL_UINT(1, mqtt->disconnect_calls);
    TEST_ASSERT_NOT_NULL(strstr(s_log_capture, "link lost"));

    fake_mqtt_port_set_fail_publish(false);
    for (i = 0u; i < 100u && mqtt->connect_calls < 2u; i++) {
        fake_time_advance_ms(200u);
        mqtt_client_step();
        app_step();
    }
    TEST_ASSERT_EQUAL_UINT(2, mqtt->connect_calls);
    TEST_ASSERT_TRUE(mqtt->connected);
}
