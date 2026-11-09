/* Tests: spec/30-processes/web-ui.md § Administration */

#include <string.h>

#include "unity.h"
#include "app_log.h"

#include "config_keys.h"
#include "fake_config_port.h"
#include "mqtt_cred.h"
#include "ntp_config.h"
#include "reboot_request.h"
#include "web_admin.h"
#include "web_api.h"
#include "wifi_cred.h"

static const config_port_t *cfg(void)
{
    return fake_config_port_get();
}

static void setup(void)
{
    fake_config_port_reset();
    reboot_request_test_reset();
    TEST_ASSERT_EQUAL(PORT_OK, wifi_cred_save(cfg(), "HomeNet", "secret123"));
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_cred_save_host(cfg(), "10.20.30.122"));
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_cred_save_pass(cfg(), "mqttpw"));
}

static int post(web_api_route_t r, const char *body, char *resp, size_t n)
{
    return web_api_handle_post(r, body, strlen(body), resp, n);
}

void test_web_admin_network_wifi_change_is_a_trial_and_reboots(void)
{
    char resp[64];
    char ssid[40];
    char pass[70];

    setup();
    TEST_ASSERT_GREATER_THAN(0, post(WEB_API_POST_NETWORK, "{\"ssid\":\"NewNet\",\"wifi_pass\":\"newpass99\"}", resp, sizeof(resp)));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    /* main credentials untouched until the new network connects */
    TEST_ASSERT_EQUAL(PORT_OK, wifi_cred_load(cfg(), ssid, sizeof(ssid), pass, sizeof(pass)));
    TEST_ASSERT_EQUAL_STRING("HomeNet", ssid);
    TEST_ASSERT_EQUAL(PORT_OK, wifi_cred_trial_load(cfg(), ssid, sizeof(ssid), pass, sizeof(pass)));
    TEST_ASSERT_EQUAL_STRING("NewNet", ssid);
    TEST_ASSERT_TRUE(reboot_request_pending());

    TEST_ASSERT_EQUAL(PORT_OK, wifi_cred_trial_commit(cfg()));
    TEST_ASSERT_EQUAL(PORT_OK, wifi_cred_load(cfg(), ssid, sizeof(ssid), pass, sizeof(pass)));
    TEST_ASSERT_EQUAL_STRING("NewNet", ssid);
    TEST_ASSERT_EQUAL(PORT_ERR_NOT_FOUND, wifi_cred_trial_load(cfg(), ssid, sizeof(ssid), pass, sizeof(pass)));
}

void test_web_admin_network_rejects_bad_fields_and_writes_nothing(void)
{
    char resp[64];
    char host[64];

    setup();
    post(WEB_API_POST_NETWORK, "{\"mqtt_host\":\"bad host\",\"ntp\":\"x.y\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"rejected\"}", resp);
    TEST_ASSERT_EQUAL(PORT_OK, cfg()->read(CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_HOST, host, sizeof(host)));
    TEST_ASSERT_EQUAL_STRING("10.20.30.122", host);
    TEST_ASSERT_FALSE(reboot_request_pending());
}

void test_web_admin_ntp_only_needs_no_reboot(void)
{
    char resp[64];
    char ntp[64];

    setup();
    post(WEB_API_POST_NETWORK, "{\"ntp\":\"10.20.10.1\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    ntp_config_get(ntp, sizeof(ntp));
    TEST_ASSERT_EQUAL_STRING("10.20.10.1", ntp);
    TEST_ASSERT_FALSE(reboot_request_pending());
}

void test_web_admin_status_never_exposes_passwords(void)
{
    char buf[400];

    setup();
    TEST_ASSERT_GREATER_THAN(0, web_admin_format_network(buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ssid\":\"HomeNet\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"mqtt_host\":\"10.20.30.122\""));
    TEST_ASSERT_NULL(strstr(buf, "secret123"));
    TEST_ASSERT_NULL(strstr(buf, "mqttpw"));
}

void test_web_admin_password_gates_posts(void)
{
    char resp[64];

    setup();
    post(WEB_API_POST_PASSWORD, "{\"password\":\"chat42\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);

    post(WEB_API_POST_NETWORK, "{\"ntp\":\"10.0.0.1\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    post(WEB_API_POST_NETWORK, "key=wrong\n{\"ntp\":\"10.0.0.1\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    post(WEB_API_POST_NETWORK, "key=chat42\n{\"ntp\":\"10.0.0.1\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);

    post(WEB_API_POST_PASSWORD, "key=chat42\n{\"password\":\"\"}", resp, sizeof(resp));
    TEST_ASSERT_FALSE(web_admin_password_set());
}

void test_web_admin_reprovision_forgets_wifi_only(void)
{
    char resp[64];
    char host[64];

    setup();
    post(WEB_API_POST_REPROVISION, "{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    TEST_ASSERT_FALSE(wifi_cred_is_stored(cfg()));
    TEST_ASSERT_EQUAL(PORT_OK, cfg()->read(CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_HOST, host, sizeof(host)));
    TEST_ASSERT_TRUE(reboot_request_pending());
}

void test_web_admin_export_import_round_trip_without_secrets(void)
{
    char buf[2048];
    char resp[64];
    char v[16];
    const uint8_t slots[4] = { 0x07, 0x1e, 0x0a, 0x01 };

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, cfg()->write(CONFIG_GROUP_FEED, CONFIG_KEY_OVERFILL_THRESHOLD_G, "120"));
    TEST_ASSERT_EQUAL(PORT_OK, cfg()->write_blob(CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_SLOTS, slots, sizeof(slots)));
    TEST_ASSERT_GREATER_THAN(0, web_api_handle_get(WEB_API_GET_EXPORT, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"feed/overfill_threshold_g\":\"120\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"schedule/slots\":\"071e0a01\""));
    TEST_ASSERT_NULL(strstr(buf, "secret123"));
    TEST_ASSERT_NULL(strstr(buf, "mqttpw"));
    TEST_ASSERT_NULL(strstr(buf, "HomeNet"));

    fake_config_port_reset();
    reboot_request_test_reset();
    post(WEB_API_POST_IMPORT, buf, resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    TEST_ASSERT_EQUAL(PORT_OK, cfg()->read(CONFIG_GROUP_FEED, CONFIG_KEY_OVERFILL_THRESHOLD_G, v, sizeof(v)));
    TEST_ASSERT_EQUAL_STRING("120", v);
    TEST_ASSERT_TRUE(reboot_request_pending());
}

/* Regression: the page sends {"password":…}; a rejected admin POST is logged. */
void test_web_admin_password_rejection_is_logged(void)
{
    char resp[64];
    char log[3072];

    fake_config_port_reset();
    post(WEB_API_POST_PASSWORD, "{\"password\":\"ab\"}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"rejected\"}", resp);
    TEST_ASSERT_GREATER_THAN(0, (int)app_log_ring_copy(log, sizeof(log)));
    TEST_ASSERT_NOT_NULL(strstr(log, "[web] /api/password rejected"));
}

void test_web_admin_unlock_route_checks_key_only(void)
{
    char resp[64];

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(WEB_API_POST_UNLOCK, web_api_classify("POST", "/api/unlock"));
    post(WEB_API_POST_UNLOCK, "{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    post(WEB_API_POST_PASSWORD, "{\"password\":\"chat42\"}", resp, sizeof(resp));
    post(WEB_API_POST_UNLOCK, "{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    post(WEB_API_POST_UNLOCK, "key=wrong\n{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    post(WEB_API_POST_UNLOCK, "key=chat42\n{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
}

static int req(const char *method, const char *path, const char *body, char *resp, size_t n)
{
    return web_api_handle_request(method, path, body, body != NULL ? strlen(body) : 0u, resp, n);
}

/* Regression: with a password set, reads must not leak over plain GET. */
void test_web_admin_password_locks_reads(void)
{
    char resp[2048];

    fake_config_port_reset();
    TEST_ASSERT_GREATER_THAN(0, req("GET", "/api/status", NULL, resp, sizeof(resp)));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"network\""));
    post(WEB_API_POST_PASSWORD, "{\"password\":\"chat42\"}", resp, sizeof(resp));

    req("GET", "/api/status", NULL, resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"locked\":true}", resp);
    req("GET", "/api/log", NULL, resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    req("GET", "/api/export", NULL, resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
    req("POST", "/api/status", "key=wrong\n", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);

    req("POST", "/api/status", "key=chat42\n", resp, sizeof(resp));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"network\""));
    req("POST", "/api/schedule/state", "key=chat42\n", resp, sizeof(resp));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"schedule\""));
    req("POST", "/api/reboot", "key=chat42\n{}", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
}

/* Regression: paths that are both read and write (/api/feed/mode, /api/config,
 * /api/feed/overfill) must serve the read when only the key line is sent. */
void test_web_admin_keyed_read_on_shared_paths(void)
{
    char resp[1024];

    fake_config_port_reset();
    post(WEB_API_POST_PASSWORD, "{\"password\":\"chat42\"}", resp, sizeof(resp));
    req("POST", "/api/feed/mode", "key=chat42\n", resp, sizeof(resp));
    TEST_ASSERT_NOT_NULL(strstr(resp, "open_loop"));
    req("POST", "/api/feed/overfill", "key=chat42\n", resp, sizeof(resp));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\"threshold_g\""));
    req("POST", "/api/config", "key=chat42\n", resp, sizeof(resp));
    TEST_ASSERT_NULL(strstr(resp, "\"error\""));
    req("POST", "/api/feed/mode", "key=chat42\ncompensated", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp);
    req("POST", "/api/feed/mode", "key=nope\n", resp, sizeof(resp));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false,\"error\":\"unauthorized\"}", resp);
}

