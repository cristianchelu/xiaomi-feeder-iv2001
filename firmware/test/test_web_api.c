/* Tests: spec/30-processes/web-ui.md */

#include <string.h>

#include "unity.h"
#include "app_log.h"

#include "bowl_mass_present.h"
#include "display_settings.h"
#include "fake_config_port.h"
#include "fake_time_port.h"
#include "feed_config.h"
#include "mqtt_battery.h"
#include "mqtt_bowl_weight.h"
#include "mqtt_hopper.h"
#include "mqtt_mains.h"
#include "mqtt_state.h"
#include "schedule.h"
#include "schedule_test_epochs.h"
#include "time_sync.h"
#include "tz_rule.h"
#include "web_api.h"

static void setup_status_synced(void)
{
    const config_port_t *cfg = fake_config_port_get();

    fake_config_port_reset();
    fake_time_port_reset();
    time_sync_test_reset();
    tz_rule_test_reset();
    schedule_test_reset();

    TEST_ASSERT_EQUAL(PORT_OK, tz_rule_save_posix(cfg, "UTC0"));
    tz_rule_init();

    fake_time_port_set_epoch(SCHEDULE_TEST_EPOCH_THU_00_00_UTC);
    time_sync_init();
    time_sync_on_wifi_ready();
    fake_time_port_queue_sync_result(true, SCHEDULE_TEST_EPOCH_THU_00_00_UTC);
    time_sync_poll(1000u);
    TEST_ASSERT_TRUE(time_sync_is_valid());

    schedule_init();
}

static void setup_status_telemetry(void)
{
    mqtt_bowl_weight_test_reset();
    mqtt_state_test_reset();
    mqtt_hopper_test_reset();
    mqtt_battery_test_reset();
    mqtt_mains_test_reset();

    mqtt_bowl_weight_sync(BOWL_MASS_KNOWN, 423, true);
    mqtt_state_sync(false);
    mqtt_hopper_sync(HOPPER_LEVEL_STATE_LOW);
    mqtt_battery_sync(true, 85u, true);
    mqtt_mains_sync(true);
}

void test_web_api_classify_status_get(void)
{
    TEST_ASSERT_EQUAL(WEB_API_GET_STATUS, web_api_classify("GET", "/api/status"));
}

void test_web_api_classify_schedule_state_get(void)
{
    TEST_ASSERT_EQUAL(WEB_API_GET_SCHEDULE_STATE,
                      web_api_classify("GET", "/api/schedule/state"));
}

void test_web_api_classify_schedule_set_post(void)
{
    TEST_ASSERT_EQUAL(WEB_API_POST_SCHEDULE_SET,
                      web_api_classify("POST", "/api/schedule/set"));
}

void test_web_api_classify_unknown_path(void)
{
    TEST_ASSERT_EQUAL(WEB_API_ROUTE_UNKNOWN, web_api_classify("GET", "/api/nope"));
}

void test_web_api_handle_get_schedule_state(void)
{
    char buf[512];
    int written;

    fake_config_port_reset();
    schedule_test_reset();
    schedule_init();

    written = web_api_handle_get(WEB_API_GET_SCHEDULE_STATE, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"schedule\""));
}

void test_web_api_handle_post_schedule_set(void)
{
    char resp[128];
    const char *payload =
        "{\"hour\":7,\"min\":15,\"repeat_days\":[0,1,2,3,4,5,6],\"g\":40,\"enabled\":true}";
    int written;

    fake_config_port_reset();
    schedule_test_reset();
    schedule_init();

    written = web_api_handle_post(WEB_API_POST_SCHEDULE_SET,
                                  payload,
                                  strlen(payload),
                                  resp,
                                  sizeof(resp));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"ok\":true"));
    TEST_ASSERT_EQUAL(1, schedule_slot_count());
}

void test_web_api_handle_get_feed_mode(void)
{
    char buf[32];
    int written;

    fake_config_port_reset();
    feed_config_mode_set(DISPENSE_MODE_COMPENSATED);

    written = web_api_handle_get(WEB_API_GET_FEED_MODE, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_EQUAL_STRING("compensated", buf);
}

void test_web_api_handle_get_feed_overfill(void)
{
    char buf[64];
    int written;

    fake_config_port_reset();
    TEST_ASSERT_TRUE(feed_config_overfill_enabled_set(true));
    TEST_ASSERT_TRUE(feed_config_overfill_threshold_g_set(40u));

    written = web_api_handle_get(WEB_API_GET_FEED_OVERFILL, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_EQUAL_STRING("{\"enabled\":true,\"threshold_g\":40}", buf);
}

void test_web_api_handle_post_feed_overfill(void)
{
    char resp[64];
    int written;

    fake_config_port_reset();
    written = web_api_handle_post(WEB_API_POST_FEED_OVERFILL,
                                  "{\"enabled\":true,\"threshold_g\":35}",
                                  strlen("{\"enabled\":true,\"threshold_g\":35}"),
                                  resp,
                                  sizeof(resp));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"ok\":true"));
    TEST_ASSERT_TRUE(feed_config_overfill_enabled_get());
    TEST_ASSERT_EQUAL_UINT8(35u, feed_config_overfill_threshold_g_get());
}

void test_web_api_handle_get_status_telemetry(void)
{
    char buf[1024];
    int written;

    setup_status_synced();
    setup_status_telemetry();

    written = web_api_handle_get(WEB_API_GET_STATUS, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, written);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"time_synced\":true"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hopper\":\"low\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bowl_weight\":\"42.3\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bowl_error\":false"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery\":\"85\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"mains\":\"ON\""));
}

void test_web_api_classify_settings_posts(void)
{
    TEST_ASSERT_EQUAL(WEB_API_POST_DISPLAY, web_api_classify("POST", "/api/display"));
    TEST_ASSERT_EQUAL(WEB_API_POST_CALIBRATE, web_api_classify("POST", "/api/calibrate"));
    TEST_ASSERT_EQUAL(WEB_API_POST_EATEN_RESET, web_api_classify("POST", "/api/eaten_reset"));
    TEST_ASSERT_EQUAL(WEB_API_POST_REBOOT, web_api_classify("POST", "/api/reboot"));
    TEST_ASSERT_EQUAL(WEB_API_POST_CHILD_LOCK, web_api_classify("POST", "/api/child_lock"));
    TEST_ASSERT_EQUAL(WEB_API_POST_POWER, web_api_classify("POST", "/api/power"));
}

void test_web_api_post_display_sets_brightness_zero(void)
{
    char resp[64];
    const char body[] = "{\"brightness\":0}";

    fake_config_port_reset();
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_post(WEB_API_POST_DISPLAY, body, strlen(body), resp, sizeof(resp)));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    TEST_ASSERT_EQUAL_UINT8(0u, display_settings_brightness_get());
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_post(WEB_API_POST_DISPLAY, "{\"mode\":\"off\"}", 14u, resp, sizeof(resp)));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"rejected\"}", resp);
}

void test_web_api_status_includes_settings(void)
{
    char buf[768];

    setup_status_synced();
    setup_status_telemetry();
    fake_config_port_reset();
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_STATUS, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"display\":{\"mode\":\"weight\",\"brightness\":4,\"battery_off_s\":60,\"mains_off_s\":0}"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"power\":{\"battery_wifi\":\"on\"}"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"child_lock\":false"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"eaten_today\":\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hopper_ir\":"));
}

void test_web_api_get_log_returns_recent_lines(void)
{
    char buf[512];

    TEST_ASSERT_EQUAL(WEB_API_GET_LOG, web_api_classify("GET", "/api/log"));
    app_log_info("test", "ring-marker-42");
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_LOG, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ring-marker-42"));
}

void test_web_api_post_dispense_amount_and_ota_base(void)
{
    char resp[64];
    char buf[1536];
    const char amt[] = "{\"g\":25}";
    const char url[] = "http://10.0.0.5:8123/local/feeder";

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(WEB_API_POST_DISPENSE_AMOUNT, web_api_classify("POST", "/api/dispense_amount"));
    TEST_ASSERT_EQUAL(WEB_API_POST_OTA_BASE, web_api_classify("POST", "/api/ota_base"));
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_post(WEB_API_POST_DISPENSE_AMOUNT, amt, strlen(amt), resp, sizeof(resp)));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_post(WEB_API_POST_OTA_BASE, url, strlen(url), resp, sizeof(resp)));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_STATUS, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"dispense_g\":25"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ota_base\":\"http://10.0.0.5:8123/local/feeder\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"progress\":{\"state\":\"idle\""));
}

/* Log ring survives a software reboot (retained RAM): web-ui.md § Log ring. */
void test_web_api_log_keeps_previous_boot(void)
{
    char buf[6144];

    app_log_info("test", "before-reboot-marker");
    app_log_test_soft_reboot();
    app_log_info("test", "after-reboot-marker");
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_LOG, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "--- previous boot ---"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "before-reboot-marker"));
    TEST_ASSERT_TRUE(strstr(buf, "--- this boot ---") < strstr(buf, "after-reboot-marker"));
    TEST_ASSERT_TRUE(strstr(buf, "before-reboot-marker") < strstr(buf, "--- this boot ---"));
}

void test_web_api_log_drops_corrupt_retained_ring(void)
{
    char buf[6144];

    app_log_info("test", "garbage-marker");
    app_log_test_corrupt_retained();
    app_log_test_soft_reboot();
    app_log_info("test", "fresh-marker");
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_LOG, buf, sizeof(buf)));
    TEST_ASSERT_NULL(strstr(buf, "--- previous boot ---"));
    TEST_ASSERT_NULL(strstr(buf, "garbage-marker"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "fresh-marker"));
}

void test_web_api_log_ring_skips_debug_lines(void)
{
    char buf[6144];

    app_log_debug("test", "debug-flood-marker");
    app_log_info("test", "info-kept-marker");
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_LOG, buf, sizeof(buf)));
    TEST_ASSERT_NULL(strstr(buf, "debug-flood-marker"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "info-kept-marker"));
}
