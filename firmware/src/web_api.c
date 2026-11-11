/*
 * LAN web API — spec/30-processes/web-ui.md
 */

#include "web_api.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "app_cmd_dispatch.h"
#include "app_lock.h"
#include "app_log.h"
#include "web_admin.h"
#include "FreeRTOS.h"
#include "task.h"
#include "mqtt_wifi_rssi.h"
#include "mqtt_dispense_amount.h"
#include "mqtt_dispense_progress.h"
#include "ota_base.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "problem.h"
#include "dispense.h"
#include "feed_config.h"
#include "eaten_today.h"
#include "feeder_runtime.h"
#include "hopper_input.h"
#include "mqtt_display.h"
#include "mqtt_eaten_today.h"
#include "mqtt_battery.h"
#include "mqtt_bowl_weight.h"
#include "mqtt_config.h"
#include "mqtt_feed_overfill.h"
#include "mqtt_hopper.h"
#include "mqtt_portion.h"
#include "mqtt_power.h"
#include "weight_port.h"
#include "mqtt_mains.h"
#include "mqtt_route.h"
#include "mqtt_state.h"
#include "port_err.h"
#include "schedule.h"
#include "time_local.h"
#include "time_sync.h"

static mqtt_route_kind_t web_api_post_route_to_mqtt(web_api_route_t route)
{
    switch (route) {
    case WEB_API_POST_SCHEDULE_SET:
        return MQTT_ROUTE_CMD_SCHEDULE_SET;
    case WEB_API_POST_SCHEDULE_DELETE:
        return MQTT_ROUTE_CMD_SCHEDULE_DELETE;
    case WEB_API_POST_SCHEDULE_TOGGLE:
        return MQTT_ROUTE_CMD_SCHEDULE_TOGGLE;
    case WEB_API_POST_SCHEDULE_SKIP:
        return MQTT_ROUTE_CMD_SCHEDULE_SKIP;
    case WEB_API_POST_SCHEDULE_ENABLE:
        return MQTT_ROUTE_CMD_SCHEDULE_ENABLE;
    case WEB_API_POST_SCHEDULE_TODAY:
        return MQTT_ROUTE_CMD_SCHEDULE_TODAY;
    case WEB_API_POST_DISPENSE:
        return MQTT_ROUTE_CMD_DISPENSE;
    case WEB_API_POST_DISPENSE_CANCEL:
        return MQTT_ROUTE_CMD_DISPENSE_CANCEL;
    case WEB_API_POST_FEED_MODE:
        return MQTT_ROUTE_CMD_FEED_MODE;
    case WEB_API_POST_FEED_OVERFILL:
        return MQTT_ROUTE_CMD_FEED_OVERFILL;
    case WEB_API_POST_CONFIG:
        return MQTT_ROUTE_CMD_CONFIG;
    case WEB_API_POST_DISPLAY:
        return MQTT_ROUTE_CMD_DISPLAY;
    case WEB_API_POST_CALIBRATE:
        return MQTT_ROUTE_CMD_CALIBRATE;
    case WEB_API_POST_EATEN_RESET:
        return MQTT_ROUTE_CMD_EATEN_RESET;
    case WEB_API_POST_REBOOT:
        return MQTT_ROUTE_CMD_REBOOT;
    case WEB_API_POST_CHILD_LOCK:
        return MQTT_ROUTE_CMD_CHILD_LOCK;
    case WEB_API_POST_POWER:
        return MQTT_ROUTE_CMD_POWER;
    case WEB_API_POST_PORTION_CAL:
        return MQTT_ROUTE_CMD_PORTION_CAL;
    case WEB_API_POST_DISPENSE_AMOUNT:
        return MQTT_ROUTE_CMD_DISPENSE_AMOUNT;
    case WEB_API_POST_PROBLEM_CLEAR:
        return MQTT_ROUTE_CMD_PROBLEM_CLEAR;
    case WEB_API_POST_FEED_LIMIT:
        return MQTT_ROUTE_CMD_FEED_LIMIT;
    case WEB_API_POST_OTA_BASE:
        return MQTT_ROUTE_CMD_OTA_BASE;
    default:
        return MQTT_ROUTE_UNKNOWN;
    }
}

static const char *web_api_hopper_wire(void)
{
    static char hopper_buf[8];

    if (mqtt_hopper_format_wire(hopper_buf, sizeof(hopper_buf))) {
        return hopper_buf;
    }

    return "normal";
}

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static const char *web_api_fw_version(void)
{
    return FW_VERSION;
}

static int web_api_write_ok(char *resp, size_t resp_len)
{
    int written;

    written = snprintf(resp, resp_len, "{\"ok\":true}");
    if (written <= 0 || (size_t)written >= resp_len) {
        return -1;
    }

    return written;
}

static int web_api_write_error(char *resp, size_t resp_len, const char *error)
{
    int written;

    if (error == NULL) {
        error = "rejected";
    }

    written = snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"%s\"}", error);
    if (written <= 0 || (size_t)written >= resp_len) {
        return -1;
    }

    return written;
}

static int web_api_append(char *buf, size_t len, int off, const char *fmt, ...)
{
    va_list ap;
    int written;

    if (off < 0 || (size_t)off >= len) {
        return -1;
    }

    va_start(ap, fmt);
    written = vsnprintf(buf + off, len - (size_t)off, fmt, ap);
    va_end(ap);

    if (written <= 0 || (size_t)off + (size_t)written >= len) {
        return -1;
    }

    return off + written;
}

static int web_api_append_telemetry_fields(char *buf, size_t len, int off)
{
    char wire[16];
    bool bowl_error;
    bool known;

    known = mqtt_bowl_weight_format_wire(wire, sizeof(wire));
    if (known) {
        off = web_api_append(buf, len, off, ",\"bowl_weight\":\"%s\"", wire);
        if (off < 0) {
            return -1;
        }
    }

    known = mqtt_state_format_bowl_error(&bowl_error);
    if (known) {
        off = web_api_append(buf,
                             len,
                             off,
                             ",\"bowl_error\":%s",
                             bowl_error ? "true" : "false");
        if (off < 0) {
            return -1;
        }
    }

    known = mqtt_battery_format_wire(wire, sizeof(wire));
    if (known) {
        off = web_api_append(buf, len, off, ",\"battery\":\"%s\"", wire);
        if (off < 0) {
            return -1;
        }
    }

    known = mqtt_mains_format_wire(wire, sizeof(wire));
    if (known) {
        off = web_api_append(buf, len, off, ",\"mains\":\"%s\"", wire);
        if (off < 0) {
            return -1;
        }
    }

    {
        char disp[112];

        if (mqtt_display_format_snapshot(disp, sizeof(disp))) {
            off = web_api_append(buf, len, off, ",\"display\":%s", disp);
            if (off < 0) {
                return -1;
            }
        }
    }

    {
        char pw[48];

        if (mqtt_power_format_snapshot(pw, sizeof(pw))) {
            off = web_api_append(buf, len, off, ",\"power\":%s", pw);
            if (off < 0) {
                return -1;
            }
        }
    }

    {
        char po[64];
        const weight_port_t *wp = weight_port_get();
        weight_cal_status_t cal = (wp != NULL && wp->get_cal_status != NULL)
                                      ? wp->get_cal_status()
                                      : WEIGHT_CAL_UNCALIBRATED;
        const char *scale = "uncalibrated";

        if (mqtt_portion_format_snapshot(po, sizeof(po))) {
            off = web_api_append(buf, len, off, ",\"portion\":%s", po);
            if (off < 0) {
                return -1;
            }
        }
        if (cal == WEIGHT_CAL_SUCCESS) {
            scale = "calibrated";
        } else if (cal == WEIGHT_CAL_CAPTURING_SPAN) {
            scale = "zero_only";
        }
        off = web_api_append(buf, len, off, ",\"scale\":\"%s\"", scale);
        if (off < 0) {
            return -1;
        }
    }

    {
        char net[320];
        int8_t rssi;

        if (web_admin_format_network(net, sizeof(net)) > 0) {
            off = web_api_append(buf, len, off, "%s", net);
            if (off < 0) {
                return -1;
            }
        }
        if (mqtt_wifi_rssi_last(&rssi)) {
            off = web_api_append(buf, len, off, ",\"rssi\":%d", (int)rssi);
            if (off < 0) {
                return -1;
            }
        }
        off = web_api_append(buf, len, off, ",\"fw\":\"%s\"", web_api_fw_version());
        if (off < 0) {
            return -1;
        }
    }

    {
        char extra[OTA_BASE_MAX_LEN + 1u];
        uint16_t amount_g = 0u;

        if (!dispense_amount_get(&amount_g)) {
            amount_g = 0u;
        }
        off = web_api_append(buf, len, off, ",\"dispense_g\":%u", (unsigned)amount_g);
        if (off < 0) {
            return -1;
        }
        if (mqtt_dispense_progress_format(extra, sizeof(extra))) {
            off = web_api_append(buf, len, off, ",\"progress\":%s", extra);
            if (off < 0) {
                return -1;
            }
        }
        if (ota_base_get(extra, sizeof(extra)) && extra[0] != '\0') {
            off = web_api_append(buf, len, off, ",\"ota_base\":\"%s\"", extra);
            if (off < 0) {
                return -1;
            }
        }
    }

    off = web_api_append(buf, len, off, ",\"child_lock\":%s",
                         feed_config_child_lock_is_active() ? "true" : "false");
    if (off < 0) {
        return -1;
    }

    if (mqtt_eaten_today_format(eaten_today_total_dg(), wire, sizeof(wire))) {
        off = web_api_append(buf, len, off, ",\"eaten_today\":\"%s\"", wire);
        if (off < 0) {
            return -1;
        }
    }

    off = web_api_append(buf, len, off, ",\"hopper_ir\":%d", (int)hopper_input_last_raw());
    if (off < 0) {
        return -1;
    }

    return off;
}

static int web_api_format_status(char *buf, size_t len)
{
    time_local_t now;
    schedule_next_t next;
    bool time_ok;
    bool have_next;
    int off;

    have_next = schedule_compute_next(&next);
    time_ok = time_sync_is_valid() && time_local_now(&now);

    if (time_ok) {
        off = snprintf(buf,
                       len,
                       "{\"time_synced\":true,"
                       "\"local_time\":\"%04u-%02u-%02u %02u:%02u:%02u\","
                       "\"hopper\":\"%s\","
                       "\"dispense_busy\":%s,"
                       "\"schedule_enabled\":%s,"
                       "\"today_enabled\":%s",
                       (unsigned)now.year,
                       (unsigned)now.month,
                       (unsigned)now.day,
                       (unsigned)now.hour,
                       (unsigned)now.min,
                       (unsigned)now.sec,
                       web_api_hopper_wire(),
                       feeder_runtime_dispense_active() ? "true" : "false",
                       schedule_global_enabled() ? "true" : "false",
                       schedule_today_enabled() ? "true" : "false");
    } else {
        off = snprintf(buf,
                       len,
                       "{\"time_synced\":false,"
                       "\"hopper\":\"%s\","
                       "\"dispense_busy\":%s,"
                       "\"schedule_enabled\":%s,"
                       "\"today_enabled\":%s",
                       web_api_hopper_wire(),
                       feeder_runtime_dispense_active() ? "true" : "false",
                       schedule_global_enabled() ? "true" : "false",
                       schedule_today_enabled() ? "true" : "false");
    }

    if (off <= 0 || (size_t)off >= len) {
        return -1;
    }

    off = web_api_append_telemetry_fields(buf, len, off);
    if (off < 0) {
        return -1;
    }

    if (time_ok && have_next) {
        off = web_api_append(buf,
                             len,
                             off,
                             ",\"next\":{\"hour\":%u,\"min\":%u,\"g\":%u,\"in_min\":%ld}",
                             (unsigned)next.hour,
                             (unsigned)next.min,
                             (unsigned)next.g,
                             (long)next.in_min);
        if (off < 0) {
            return -1;
        }
    }

    off = web_api_append(buf, len, off, "}");
    return off < 0 ? -1 : off;
}

static int web_api_unauthorized(const char *path, char *resp, size_t resp_len);

static const struct {
    const char *path;
    web_api_route_t route;
} s_post_routes[] = {
    { "/api/schedule/set", WEB_API_POST_SCHEDULE_SET },
    { "/api/schedule/delete", WEB_API_POST_SCHEDULE_DELETE },
    { "/api/schedule/toggle", WEB_API_POST_SCHEDULE_TOGGLE },
    { "/api/schedule/skip", WEB_API_POST_SCHEDULE_SKIP },
    { "/api/schedule/enable", WEB_API_POST_SCHEDULE_ENABLE },
    { "/api/schedule/today", WEB_API_POST_SCHEDULE_TODAY },
    { "/api/dispense", WEB_API_POST_DISPENSE },
    { "/api/dispense/cancel", WEB_API_POST_DISPENSE_CANCEL },
    { "/api/feed/mode", WEB_API_POST_FEED_MODE },
    { "/api/feed/overfill", WEB_API_POST_FEED_OVERFILL },
    { "/api/config", WEB_API_POST_CONFIG },
    { "/api/display", WEB_API_POST_DISPLAY },
    { "/api/calibrate", WEB_API_POST_CALIBRATE },
    { "/api/eaten_reset", WEB_API_POST_EATEN_RESET },
    { "/api/reboot", WEB_API_POST_REBOOT },
    { "/api/child_lock", WEB_API_POST_CHILD_LOCK },
    { "/api/power", WEB_API_POST_POWER },
    { "/api/portion_cal", WEB_API_POST_PORTION_CAL },
    { "/api/network", WEB_API_POST_NETWORK },
    { "/api/password", WEB_API_POST_PASSWORD },
    { "/api/reprovision", WEB_API_POST_REPROVISION },
    { "/api/factory_reset", WEB_API_POST_FACTORY_RESET },
    { "/api/import", WEB_API_POST_IMPORT },
    { "/api/dispense_amount", WEB_API_POST_DISPENSE_AMOUNT },
    { "/api/problem_clear", WEB_API_POST_PROBLEM_CLEAR },
    { "/api/feed_limit", WEB_API_POST_FEED_LIMIT },
    { "/api/ota_base", WEB_API_POST_OTA_BASE },
    { "/api/unlock", WEB_API_POST_UNLOCK },
};

#define POST_ROUTE_COUNT (sizeof(s_post_routes) / sizeof(s_post_routes[0]))

static const char *web_api_post_path(web_api_route_t route)
{
    size_t i;

    for (i = 0u; i < POST_ROUTE_COUNT; i++) {
        if (s_post_routes[i].route == route) {
            return s_post_routes[i].path;
        }
    }
    return "?";
}

web_api_route_t web_api_classify(const char *method, const char *path)
{
    bool is_get;
    bool is_post;

    if (method == NULL || path == NULL) {
        return WEB_API_ROUTE_UNKNOWN;
    }

    is_get = strcmp(method, "GET") == 0;
    is_post = strcmp(method, "POST") == 0;

    if (!is_get && !is_post) {
        return WEB_API_ROUTE_UNKNOWN;
    }

    if (is_get) {
        if (strcmp(path, "/api/status") == 0) {
            return WEB_API_GET_STATUS;
        }
        if (strcmp(path, "/api/log") == 0) {
            return WEB_API_GET_LOG;
        }
        if (strcmp(path, "/api/history") == 0) {
            return WEB_API_GET_HISTORY;
        }
        if (strcmp(path, "/api/export") == 0) {
            return WEB_API_GET_EXPORT;
        }
        if (strcmp(path, "/api/schedule/state") == 0) {
            return WEB_API_GET_SCHEDULE_STATE;
        }
        if (strcmp(path, "/api/schedule/next") == 0) {
            return WEB_API_GET_SCHEDULE_NEXT;
        }
        if (strcmp(path, "/api/config") == 0) {
            return WEB_API_GET_CONFIG;
        }
        if (strcmp(path, "/api/feed/mode") == 0) {
            return WEB_API_GET_FEED_MODE;
        }
        if (strcmp(path, "/api/feed/overfill") == 0) {
            return WEB_API_GET_FEED_OVERFILL;
        }

        return WEB_API_ROUTE_UNKNOWN;
    }

    for (size_t i = 0u; i < POST_ROUTE_COUNT; i++) {
        if (strcmp(path, s_post_routes[i].path) == 0) {
            return s_post_routes[i].route;
        }
    }

    return WEB_API_ROUTE_UNKNOWN;
}

int web_api_handle_get(web_api_route_t route, char *buf, size_t len)
{
    int written;

    if (buf == NULL || len == 0) {
        return -1;
    }

    switch (route) {
    case WEB_API_GET_SCHEDULE_STATE:
        return schedule_format_state_json(buf, len);

    case WEB_API_GET_SCHEDULE_NEXT:
        written = schedule_format_next_json(buf, len);
        if (written > 0) {
            return written;
        }

        if (len < 3) {
            return -1;
        }

        buf[0] = '{';
        buf[1] = '}';
        buf[2] = '\0';
        return 2;

    case WEB_API_GET_CONFIG:
        if (!mqtt_config_format_snapshot(buf, len)) {
            return -1;
        }

        return (int)strlen(buf);

    case WEB_API_GET_FEED_MODE:
        written = snprintf(buf,
                           len,
                           "%s",
                           feed_config_mode_string(feed_config_mode_get()));
        if (written <= 0 || (size_t)written >= len) {
            return -1;
        }

        return written;

    case WEB_API_GET_FEED_OVERFILL:
        if (!mqtt_feed_overfill_format_snapshot(buf, len)) {
            return -1;
        }

        return (int)strlen(buf);

    case WEB_API_GET_STATUS:
        return web_api_format_status(buf, len);

    case WEB_API_GET_LOG: {
        size_t off = 0u;

        if (len < 64u) {
            return -1;
        }
        {
            static const char prev_hdr[] = "--- previous boot ---\n";
            static const char this_hdr[] = "--- this boot ---\n";
            size_t room = len / 2u;
            size_t n;

            if (room > sizeof(prev_hdr)) {
                n = app_log_prev_copy(buf + sizeof(prev_hdr) - 1u, room - (sizeof(prev_hdr) - 1u));
                if (n > 0u) {
                    memcpy(buf, prev_hdr, sizeof(prev_hdr) - 1u);
                    off = sizeof(prev_hdr) - 1u + n;
                    if (buf[off - 1u] != '\n') {
                        buf[off++] = '\n';
                    }
                }
            }
            memcpy(buf + off, this_hdr, sizeof(this_hdr) - 1u);
            off += sizeof(this_hdr) - 1u;
            off += app_log_ring_copy(buf + off, len - off);
        }
        return (int)off;
    }

    case WEB_API_GET_HISTORY:
        return feed_history_format_json(buf, len);

    case WEB_API_GET_EXPORT:
        return web_admin_export(buf, len);

    default:
        return -1;
    }
}

static int web_api_dispatch_post(web_api_route_t route,
                                 const char *body,
                                 size_t body_len,
                                 char *resp,
                                 size_t resp_len);

int web_api_handle_post(web_api_route_t route,
                        const char *body,
                        size_t body_len,
                        char *resp,
                        size_t resp_len)
{
    if (resp == NULL || resp_len == 0) {
        return -1;
    }

    /* Optional admin password: first body line "key=<password>". */
    if (!web_admin_unlock(&body, &body_len)) {
        return web_api_unauthorized(web_api_post_path(route), resp, resp_len);
    }
    return web_api_dispatch_post(route, body, body_len, resp, resp_len);
}

/* Body already unlocked (key line stripped). */
static int web_api_dispatch_post(web_api_route_t route,
                                 const char *body,
                                 size_t body_len,
                                 char *resp,
                                 size_t resp_len)
{
    mqtt_route_kind_t mqtt_route;
    port_err_t err;
    web_admin_action_t admin;
    bool is_admin = true;

    if (route == WEB_API_POST_UNLOCK) {
        return web_api_write_ok(resp, resp_len);
    }

    switch (route) {
    case WEB_API_POST_NETWORK:
        admin = WEB_ADMIN_NETWORK;
        break;
    case WEB_API_POST_PASSWORD:
        admin = WEB_ADMIN_PASSWORD;
        break;
    case WEB_API_POST_REPROVISION:
        admin = WEB_ADMIN_REPROVISION;
        break;
    case WEB_API_POST_FACTORY_RESET:
        admin = WEB_ADMIN_FACTORY_RESET;
        break;
    case WEB_API_POST_IMPORT:
        admin = WEB_ADMIN_IMPORT;
        break;
    default:
        is_admin = false;
        admin = WEB_ADMIN_NETWORK;
        break;
    }
    if (is_admin) {
        if (!app_lock_take(3000u)) {
            return web_api_write_error(resp, resp_len, "busy");
        }
        err = web_admin_run(admin, body, body_len);
        app_lock_give();
        if (err == PORT_OK) {
            return web_api_write_ok(resp, resp_len);
        }
        app_log_info("web", "%s rejected (%s)", web_api_post_path(route),
                     err == PORT_ERR_INVALID_ARG ? "invalid" : "io");
        return web_api_write_error(resp, resp_len,
                                   err == PORT_ERR_INVALID_ARG ? "rejected" : "io");
    }

    mqtt_route = web_api_post_route_to_mqtt(route);
    if (mqtt_route == MQTT_ROUTE_UNKNOWN) {
        return web_api_write_error(resp, resp_len, "unknown_route");
    }

    /* Web server task: serialise with the app task (shared feeder state). */
    if (!app_lock_take(3000u)) {
        return web_api_write_error(resp, resp_len, "busy");
    }
    err = app_cmd_dispatch(mqtt_route, body, body_len, "web");
    app_lock_give();
    if (err == PORT_OK) {
        return web_api_write_ok(resp, resp_len);
    }

    app_log_info("web", "%s rejected (err %d)", web_api_post_path(route), (int)err);
    if (err == PORT_ERR_BUSY) {
        return web_api_write_error(resp, resp_len, "busy");
    }

    if (err == PORT_ERR_NOT_SUPPORTED) {
        return web_api_write_error(resp, resp_len, "not_supported");
    }

    return web_api_write_error(resp, resp_len, "rejected");
}

/* Failed key: slow down password guessing (web-ui.md § Security). */
static int web_api_unauthorized(const char *path, char *resp, size_t resp_len)
{
    app_log_info("web", "%s rejected (unauthorized)", path);
    vTaskDelay(pdMS_TO_TICKS(WEB_API_AUTH_FAIL_DELAY_MS));
    return web_api_write_error(resp, resp_len, "unauthorized");
}

int web_api_handle_request(const char *method,
                           const char *path,
                           const char *body,
                           size_t body_len,
                           char *resp,
                           size_t resp_len)
{
    web_api_route_t route;

    if (method == NULL || path == NULL || resp == NULL || resp_len == 0u) {
        return -1;
    }

    if (strcmp(method, "GET") == 0) {
        route = web_api_classify("GET", path);
        if (route == WEB_API_ROUTE_UNKNOWN) {
            return snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"not_found\"}");
        }
        if (web_admin_password_set()) {
            if (route == WEB_API_GET_STATUS) {
                return snprintf(resp, resp_len, "{\"locked\":true}");
            }
            return web_api_write_error(resp, resp_len, "unauthorized");
        }
        return web_api_handle_get(route, resp, resp_len);
    }

    if (!web_admin_unlock(&body, &body_len)) {
        return web_api_unauthorized(path, resp, resp_len);
    }

    /* Empty body after the key line on a read path: authenticated read. */
    route = web_api_classify("GET", path);
    if (route != WEB_API_ROUTE_UNKNOWN && body_len == 0u) {
        return web_api_handle_get(route, resp, resp_len);
    }

    route = web_api_classify("POST", path);
    if (route == WEB_API_ROUTE_UNKNOWN) {
        return snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"not_found\"}");
    }
    return web_api_dispatch_post(route, body, body_len, resp, resp_len);
}
