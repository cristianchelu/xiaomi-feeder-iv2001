/*
 * OTA firmware base URL — spec/30-processes/ota-flow.md § One-button install
 */

#include "ota_base.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "config_keys.h"
#include "config_port.h"
#include "mqtt_outbox.h"
#include "mqtt_topics.h"

#define OTA_BASE_KEY  "ota_base"

static char s_topic[96];

bool ota_base_get(char *buf, size_t len)
{
    const config_port_t *cfg = config_port_get();

    if (cfg == NULL || buf == NULL || len == 0u ||
        cfg->read(CONFIG_GROUP_SYSTEM, OTA_BASE_KEY, buf, len) != PORT_OK) {
        return false;
    }
    return buf[0] != '\0';
}

bool ota_base_build_url(char bank_letter, char *buf, size_t len)
{
    char base[OTA_BASE_MAX_LEN + 1u];
    int written;

    if (buf == NULL || (bank_letter != 'a' && bank_letter != 'b') ||
        !ota_base_get(base, sizeof(base))) {
        return false;
    }
    written = snprintf(buf, len, "%s/petfeeder_%c.bin", base, bank_letter);
    return written > 0 && (size_t)written < len;
}

void ota_base_set_device_id(const char *device_id)
{
    if (device_id == NULL || device_id[0] == '\0' ||
        mqtt_topic_format(s_topic, sizeof(s_topic), device_id, "ota/base") != PORT_OK) {
        s_topic[0] = '\0';
    }
}

static void ota_base_publish(void)
{
    char base[OTA_BASE_MAX_LEN + 1u];

    if (s_topic[0] == '\0') {
        return;
    }
    if (!ota_base_get(base, sizeof(base))) {
        base[0] = '\0';
    }
    (void)mqtt_outbox_enqueue(s_topic, base, strlen(base), 1, true);
}

void ota_base_connect_snapshot(void)
{
    char base[OTA_BASE_MAX_LEN + 1u];

    if (ota_base_get(base, sizeof(base))) {
        ota_base_publish();
    }
}

port_err_t ota_base_handle(const void *payload, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char buf[OTA_BASE_MAX_LEN + 1u];

    if (payload == NULL || cfg == NULL || len == 0u || len > OTA_BASE_MAX_LEN) {
        return PORT_ERR_INVALID_ARG;
    }
    memcpy(buf, payload, len);
    buf[len] = '\0';
    while (len > 0u && (buf[len - 1u] == '/' || buf[len - 1u] == ' ' ||
                        buf[len - 1u] == '\n' || buf[len - 1u] == '\r')) {
        buf[--len] = '\0';
    }
    if (strncmp(buf, "http://", 7u) != 0 && strncmp(buf, "https://", 8u) != 0) {
        return PORT_ERR_INVALID_ARG;
    }
    if (len < 12u || cfg->write(CONFIG_GROUP_SYSTEM, OTA_BASE_KEY, buf) != PORT_OK) {
        return PORT_ERR_INVALID_ARG;
    }
    app_log_info("ota", "base url set");
    ota_base_publish();
    return PORT_OK;
}

void ota_base_test_reset(void)
{
    s_topic[0] = '\0';
}
