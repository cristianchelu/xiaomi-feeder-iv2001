/*
 * Web admin: network settings, password, provisioning, backup —
 * spec/30-processes/web-ui.md § Administration
 */

#include "web_admin.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "app_log.h"
#include "config_keys.h"
#include "config_port.h"
#include "mqtt_cred.h"
#include "mqtt_json.h"
#include "ntp_config.h"
#include "provision_reset.h"
#include "reboot_request.h"
#include "time_sync.h"
#include "wifi_cred.h"
#ifndef HOST_TEST
#include "wifi_sdk_profile.h"
#endif

#define WEB_PASS_KEY "web_pass"

/* Settings carried by export/import (strings). Wi-Fi and every password stay
 * out: a restore never locks the feeder off the network. */
static const struct {
    const char *group;
    const char *key;
} s_backup_keys[] = {
    { CONFIG_GROUP_FEED, CONFIG_KEY_FEED_MODE },
    { CONFIG_GROUP_FEED, CONFIG_KEY_CHILD_LOCK },
    { CONFIG_GROUP_FEED, CONFIG_KEY_OVERFILL_ENABLED },
    { CONFIG_GROUP_FEED, CONFIG_KEY_OVERFILL_THRESHOLD_G },
    { CONFIG_GROUP_FEED, "portion_dg" },
    { CONFIG_GROUP_FEED, "button_g" },
    { CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE },
    { CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BRIGHTNESS },
    { CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BATTERY_OFF_S },
    { CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MAINS_OFF_S },
    { CONFIG_GROUP_POWER, CONFIG_KEY_POWER_BATTERY_WIFI },
    { CONFIG_GROUP_TIME, CONFIG_KEY_TZ_RULE },
    { CONFIG_GROUP_TIME, CONFIG_KEY_TZ_LABEL },
    { CONFIG_GROUP_TIME, CONFIG_KEY_NTP_SERVER },
    { CONFIG_GROUP_SYSTEM, "ota_base" },
    { CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_HOST },
    { CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_PORT },
    { CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_USER },
    { CONFIG_GROUP_CALIB, CONFIG_KEY_CALIB_ZERO },
    { CONFIG_GROUP_CALIB, CONFIG_KEY_CALIB_SPAN_G },
    { CONFIG_GROUP_CALIB, CONFIG_KEY_CALIB_SPAN_RAW },
    { CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_ENABLED },
};

#define BACKUP_COUNT (sizeof(s_backup_keys) / sizeof(s_backup_keys[0]))
#define SCHEDULE_BLOB_MAX 256u

/* Hostname or IPv4: letters, digits, '.' and '-' only. */
static bool web_admin_hostname_ok(const char *s)
{
    size_t i;

    if (s == NULL || s[0] == '\0' || s[0] == '.' || s[0] == '-') {
        return false;
    }
    for (i = 0u; s[i] != '\0'; i++) {
        char c = s[i];

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return false;
        }
    }
    return true;
}

static void web_admin_invalidate_sdk_profile(void)
{
#ifndef HOST_TEST
    wifi_sdk_profile_invalidate();
#endif
}

static bool web_admin_read_pass(char *buf, size_t len)
{
    const config_port_t *cfg = config_port_get();

    return cfg != NULL && cfg->read(CONFIG_GROUP_SYSTEM, WEB_PASS_KEY, buf, len) == PORT_OK &&
           buf[0] != '\0';
}

bool web_admin_password_set(void)
{
    char pass[WEB_ADMIN_PASS_MAX + 1u];

    return web_admin_read_pass(pass, sizeof(pass));
}

bool web_admin_unlock(const char **body, size_t *len)
{
    char pass[WEB_ADMIN_PASS_MAX + 1u];
    const char *b;
    const char *nl;
    size_t n;
    bool has_line;
    bool ok;

    if (body == NULL || len == NULL) {
        return false;
    }
    b = *body;
    n = *len;
    has_line = b != NULL && n >= 4u && strncmp(b, "key=", 4u) == 0;
    nl = has_line ? memchr(b, '\n', n) : NULL;

    if (!web_admin_read_pass(pass, sizeof(pass))) {
        ok = true;
    } else if (nl == NULL) {
        ok = false;
    } else {
        size_t klen = (size_t)(nl - (b + 4));

        ok = klen == strlen(pass) && memcmp(b + 4, pass, klen) == 0;
    }

    if (nl != NULL) {
        *len = n - (size_t)(nl + 1 - b);
        *body = nl + 1;
    }
    return ok;
}

/* "key":"value" string member into buf; false when absent or too long. */
static bool web_admin_field(const char *body, size_t len, const char *key, char *buf, size_t buf_len)
{
    return mqtt_json_find_string(body, len, key, buf, buf_len);
}

static port_err_t web_admin_network(const char *body, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char wpass[WIFI_PASS_MAX_LEN + 1];
    char host[MQTT_HOST_MAX_LEN + 1];
    char user[MQTT_USER_MAX_LEN + 1];
    char mpass[MQTT_PASS_MAX_LEN + 1];
    char ntp[NTP_CONFIG_MAX_LEN + 1];
    unsigned port = 0u;
    bool reboot = false;

    if (cfg == NULL || body == NULL || len == 0u) {
        return PORT_ERR_INVALID_ARG;
    }

    /* Validate everything first; nothing is written on a bad field. */
    if (web_admin_field(body, len, "ssid", ssid, sizeof(ssid))) {
        if (!web_admin_field(body, len, "wifi_pass", wpass, sizeof(wpass))) {
            char cur[WIFI_SSID_MAX_LEN + 1];

            /* Same network, password unchanged: keep it. */
            if (wifi_cred_load(cfg, cur, sizeof(cur), wpass, sizeof(wpass)) != PORT_OK ||
                strcmp(cur, ssid) != 0) {
                wpass[0] = '\0';
            }
        }
        if (wifi_cred_validate(ssid, wpass) != PORT_OK) {
            return PORT_ERR_INVALID_ARG;
        }
    } else {
        ssid[0] = '\0';
    }
    if (web_admin_field(body, len, "mqtt_host", host, sizeof(host))) {
        if (host[0] != '\0' &&
            (!web_admin_hostname_ok(host) || mqtt_cred_validate_host(host) != PORT_OK)) {
            return PORT_ERR_INVALID_ARG;
        }
    } else {
        host[0] = '\0';
    }
    if (mqtt_json_find_uint(body, len, "mqtt_port", &port) &&
        (port > 65535u || mqtt_cred_validate_port((uint16_t)port) != PORT_OK)) {
        return PORT_ERR_INVALID_ARG;
    }
    if (web_admin_field(body, len, "ntp", ntp, sizeof(ntp))) {
        if (ntp[0] != '\0' && ntp_config_set(ntp) != PORT_OK) {
            return PORT_ERR_INVALID_ARG;
        }
        if (ntp[0] == '\0') {
            (void)ntp_config_set("");
        }
        (void)time_sync_request_now();
    }

    if (host[0] != '\0') {
        if (mqtt_cred_save_host(cfg, host) != PORT_OK) {
            return PORT_ERR_IO;
        }
        reboot = true;
    }
    if (port != 0u) {
        if (mqtt_cred_save_port(cfg, (uint16_t)port) != PORT_OK) {
            return PORT_ERR_IO;
        }
        reboot = true;
    }
    if (mqtt_json_has_key(body, len, "mqtt_user") &&
        web_admin_field(body, len, "mqtt_user", user, sizeof(user))) {
        if (mqtt_cred_save_user(cfg, user) != PORT_OK) {
            return PORT_ERR_IO;
        }
        reboot = true;
    }
    if (web_admin_field(body, len, "mqtt_pass", mpass, sizeof(mpass))) {
        if (mqtt_cred_save_pass(cfg, mpass) != PORT_OK) {
            return PORT_ERR_IO;
        }
        reboot = true;
    }
    if (ssid[0] != '\0') {
        if (wifi_cred_trial_save(cfg, ssid, wpass) != PORT_OK) {
            return PORT_ERR_IO;
        }
        app_log_info("web", "new Wi-Fi \"%s\" saved for trial", ssid);
        reboot = true;
    }
    if (reboot) {
        (void)reboot_request_arm();
    }
    return PORT_OK;
}

static port_err_t web_admin_password(const char *body, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char pass[WEB_ADMIN_PASS_MAX + 1u];

    if (cfg == NULL || body == NULL ||
        !web_admin_field(body, len, "new", pass, sizeof(pass))) {
        return PORT_ERR_INVALID_ARG;
    }
    if (pass[0] == '\0') {
        (void)cfg->erase(CONFIG_GROUP_SYSTEM, WEB_PASS_KEY);
        app_log_info("web", "admin password removed");
        return PORT_OK;
    }
    if (strlen(pass) < 4u || strchr(pass, '\n') != NULL) {
        return PORT_ERR_INVALID_ARG;
    }
    app_log_info("web", "admin password set");
    return cfg->write(CONFIG_GROUP_SYSTEM, WEB_PASS_KEY, pass);
}

static port_err_t web_admin_reprovision(void)
{
    const config_port_t *cfg = config_port_get();

    if (cfg == NULL) {
        return PORT_ERR_IO;
    }
    (void)cfg->erase(CONFIG_GROUP_WIFI, CONFIG_KEY_WIFI_SSID);
    (void)cfg->erase(CONFIG_GROUP_WIFI, CONFIG_KEY_WIFI_PASS);
    wifi_cred_trial_drop(cfg);
    web_admin_invalidate_sdk_profile();
    app_log_info("web", "re-provisioning: Wi-Fi forgotten, rebooting to setup AP");
    (void)reboot_request_arm();
    return PORT_OK;
}

static port_err_t web_admin_factory_reset(void)
{
    if (!provision_erase_app_groups(config_port_get())) {
        return PORT_ERR_IO;
    }
    web_admin_invalidate_sdk_profile();
    app_log_info("web", "factory reset — rebooting");
    (void)reboot_request_arm();
    return PORT_OK;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static port_err_t web_admin_import(const char *body, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char name[48];
    char value[SCHEDULE_BLOB_MAX * 2u + 1u];
    size_t i;
    unsigned applied = 0u;

    if (cfg == NULL || body == NULL || len == 0u) {
        return PORT_ERR_INVALID_ARG;
    }
    for (i = 0u; i < BACKUP_COUNT; i++) {
        (void)snprintf(name, sizeof(name), "%s/%s", s_backup_keys[i].group, s_backup_keys[i].key);
        if (mqtt_json_find_string(body, len, name, value, sizeof(value))) {
            if (cfg->write(s_backup_keys[i].group, s_backup_keys[i].key, value) == PORT_OK) {
                applied++;
            }
        }
    }
    (void)snprintf(name, sizeof(name), "%s/%s", CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_SLOTS);
    if (cfg->write_blob != NULL && mqtt_json_find_string(body, len, name, value, sizeof(value))) {
        uint8_t blob[SCHEDULE_BLOB_MAX];
        size_t n = strlen(value) / 2u;
        size_t k;
        bool ok = (strlen(value) % 2u) == 0u && n <= sizeof(blob);

        for (k = 0u; ok && k < n; k++) {
            int hi = hexval(value[2u * k]);
            int lo = hexval(value[2u * k + 1u]);

            ok = hi >= 0 && lo >= 0;
            blob[k] = (uint8_t)((hi << 4) | (lo & 0xF));
        }
        if (ok && cfg->write_blob(CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_SLOTS, blob, n) == PORT_OK) {
            applied++;
        }
    }
    if (applied == 0u) {
        return PORT_ERR_INVALID_ARG;
    }
    app_log_info("web", "settings restored (%u keys) — rebooting", applied);
    (void)reboot_request_arm();
    return PORT_OK;
}

port_err_t web_admin_run(web_admin_action_t action, const char *body, size_t len)
{
    switch (action) {
    case WEB_ADMIN_NETWORK:
        return web_admin_network(body, len);
    case WEB_ADMIN_PASSWORD:
        return web_admin_password(body, len);
    case WEB_ADMIN_REPROVISION:
        return web_admin_reprovision();
    case WEB_ADMIN_FACTORY_RESET:
        return web_admin_factory_reset();
    case WEB_ADMIN_IMPORT:
        return web_admin_import(body, len);
    default:
        return PORT_ERR_INVALID_ARG;
    }
}

/* JSON-escape a short string into out (", \\ and control characters). */
static void web_admin_escape(const char *in, char *out, size_t len)
{
    size_t o = 0u;

    while (*in != '\0' && o + 2u < len) {
        unsigned char c = (unsigned char)*in++;

        if (c == '"' || c == '\\') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c >= 0x20u) {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

int web_admin_format_network(char *buf, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char ssid[WIFI_SSID_MAX_LEN + 1] = "";
    char host[96] = "";
    char user[MQTT_USER_MAX_LEN + 1] = "";
    char port[8] = "1883";
    char ntp[NTP_CONFIG_MAX_LEN + 1];
    char e_ssid[2 * WIFI_SSID_MAX_LEN + 1];
    char e_host[2 * sizeof(host)];
    char e_user[2 * MQTT_USER_MAX_LEN + 1];

    if (cfg != NULL) {
        (void)cfg->read(CONFIG_GROUP_WIFI, CONFIG_KEY_WIFI_SSID, ssid, sizeof(ssid));
        (void)cfg->read(CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_HOST, host, sizeof(host));
        (void)cfg->read(CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_USER, user, sizeof(user));
        (void)cfg->read(CONFIG_GROUP_MQTT, CONFIG_KEY_MQTT_PORT, port, sizeof(port));
    }
    ntp_config_get(ntp, sizeof(ntp));
    web_admin_escape(ssid, e_ssid, sizeof(e_ssid));
    web_admin_escape(host, e_host, sizeof(e_host));
    web_admin_escape(user, e_user, sizeof(e_user));
    return snprintf(buf, len,
                    ",\"network\":{\"ssid\":\"%s\",\"mqtt_host\":\"%s\",\"mqtt_port\":%ld,"
                    "\"mqtt_user\":\"%s\",\"ntp\":\"%s\",\"password\":%s}",
                    e_ssid, e_host, strtol(port, NULL, 10), e_user, ntp,
                    web_admin_password_set() ? "true" : "false");
}

int web_admin_export(char *buf, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char value[96];
    char esc[192];
    size_t i;
    int off = 0;
    int w;

    if (cfg == NULL || buf == NULL || len < 4u) {
        return -1;
    }
    w = snprintf(buf, len, "{\"format\":\"petfeeder-settings-1\"");
    if (w < 0 || (size_t)w >= len) {
        return -1;
    }
    off = w;
    for (i = 0u; i < BACKUP_COUNT; i++) {
        if (cfg->read(s_backup_keys[i].group, s_backup_keys[i].key, value, sizeof(value)) != PORT_OK) {
            continue;
        }
        web_admin_escape(value, esc, sizeof(esc));
        w = snprintf(buf + off, len - (size_t)off, ",\"%s/%s\":\"%s\"",
                     s_backup_keys[i].group, s_backup_keys[i].key, esc);
        if (w < 0 || (size_t)w >= len - (size_t)off) {
            return -1;
        }
        off += w;
    }
    if (cfg->read_blob != NULL) {
        uint8_t blob[SCHEDULE_BLOB_MAX];
        size_t n = 0u;
        size_t k;

        if (cfg->read_blob(CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_SLOTS, blob, sizeof(blob), &n) == PORT_OK &&
            n > 0u && (size_t)off + 2u * n + 32u < len) {
            off += snprintf(buf + off, len - (size_t)off, ",\"%s/%s\":\"",
                            CONFIG_GROUP_SCHEDULE, CONFIG_KEY_SCHEDULE_SLOTS);
            for (k = 0u; k < n; k++) {
                off += snprintf(buf + off, len - (size_t)off, "%02x", blob[k]);
            }
            off += snprintf(buf + off, len - (size_t)off, "\"");
        }
    }
    w = snprintf(buf + off, len - (size_t)off, "}");
    if (w < 0 || (size_t)w >= len - (size_t)off) {
        return -1;
    }
    return off + w;
}
