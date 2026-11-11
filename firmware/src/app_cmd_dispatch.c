/*
 * Shared command dispatch — spec/30-processes/web-ui.md, mqtt-protocol.md
 */

#include "app_cmd_dispatch.h"

#include <string.h>

#include "dispense.h"
#include "eaten_today.h"
#include "mqtt_eaten_today.h"
#include "mqtt_config.h"
#include "mqtt_dispense_cmd.h"
#include "app.h"
#include "mqtt_calibrate.h"
#include "mqtt_child_lock.h"
#include "mqtt_display.h"
#include "mqtt_feed_mode.h"
#include "mqtt_feed_overfill.h"
#include "mqtt_json.h"
#include "mqtt_dispense_amount.h"
#include "mqtt_problem.h"
#include "feed_limit.h"
#include "mqtt_portion.h"
#include "mqtt_power.h"
#include "ota_base.h"
#include "mqtt_schedule.h"
#include "reboot_request.h"
#include "schedule.h"
#include "schedule_cmd.h"

port_err_t app_cmd_dispatch(mqtt_route_kind_t route,
                            const void *payload,
                            size_t len,
                            const char *device_id)
{
    const char *body = payload;
    unsigned grams = 0u;

    switch (route) {
    case MQTT_ROUTE_CMD_DISPENSE:
        if (body != NULL && len > 0u) {
            if (mqtt_json_find_uint(body, len, "g", &grams)) {
                if (grams >= SCHEDULE_G_MIN && grams <= SCHEDULE_G_MAX) {
                    if (dispense_submit_grams((uint8_t)grams, DISPENSE_SOURCE_MQTT)
                            == DISPENSE_SUBMIT_OK) {
                        return PORT_OK;
                    }

                    return PORT_ERR_BUSY;
                }

                return PORT_ERR_INVALID_ARG;
            }

            if (mqtt_json_has_key(body, len, "g")) {
                return PORT_ERR_INVALID_ARG;
            }
        }

        {
            uint16_t amount_g;

            /* Dispense button: the configured amount, else one portion. */
            if (dispense_amount_get(&amount_g)) {
                return dispense_submit_grams((uint8_t)amount_g, DISPENSE_SOURCE_MQTT)
                               == DISPENSE_SUBMIT_OK
                           ? PORT_OK
                           : PORT_ERR_BUSY;
            }
        }
        if (dispense_submit_portions((uint8_t)MQTT_DISPENSE_DEFAULT_PORTIONS,
                                     DISPENSE_SOURCE_MQTT)
                == DISPENSE_SUBMIT_OK) {
            return PORT_OK;
        }

        return PORT_ERR_BUSY;

    case MQTT_ROUTE_CMD_DISPENSE_CANCEL:
        if (dispense_is_active()) {
            return PORT_ERR_NOT_SUPPORTED;
        }

        return PORT_OK;

    case MQTT_ROUTE_CMD_CONFIG:
        return mqtt_config_handle(payload, len);

    case MQTT_ROUTE_CMD_FEED_MODE:
        return mqtt_feed_mode_handle(payload, len);

    case MQTT_ROUTE_CMD_FEED_OVERFILL:
        return mqtt_feed_overfill_handle(payload, len);

    case MQTT_ROUTE_CMD_CHILD_LOCK: {
        port_err_t err = mqtt_child_lock_handle(payload, len);

        if (err == PORT_OK) {
            app_child_lock_refresh_display();
        }

        return err;
    }

    case MQTT_ROUTE_CMD_CALIBRATE: {
        port_err_t err = mqtt_calibrate_handle(payload, len);

        if (err == PORT_OK) {
            app_weight_notify_calibration_changed();
        }

        return err;
    }

    case MQTT_ROUTE_CMD_DISPLAY: {
        port_err_t err = mqtt_display_handle(payload, len);

        if (err == PORT_OK) {
            app_display_apply_settings();
        }

        return err;
    }

    case MQTT_ROUTE_CMD_POWER:
        return mqtt_power_handle(payload, len);

    case MQTT_ROUTE_CMD_PORTION_CAL:
        return mqtt_portion_handle(payload, len);

    case MQTT_ROUTE_CMD_DISPENSE_AMOUNT:
        return mqtt_dispense_amount_handle(payload, len);

    case MQTT_ROUTE_CMD_PROBLEM_CLEAR:
        return mqtt_problem_handle_clear();

    case MQTT_ROUTE_CMD_SCHEDULE_FEED_NEXT: {
        schedule_feed_next_t r = schedule_feed_next();

        if (r == SCHEDULE_FEED_NEXT_OK) {
            mqtt_schedule_request_publish();
            return PORT_OK;
        }
        return r == SCHEDULE_FEED_NEXT_BUSY ? PORT_ERR_BUSY : PORT_ERR_NOT_SUPPORTED;
    }

    case MQTT_ROUTE_CMD_FEED_LIMIT:
        return mqtt_feed_limit_handle(payload, len);

    case MQTT_ROUTE_CMD_OTA_BASE:
        return ota_base_handle(payload, len);

    case MQTT_ROUTE_CMD_EATEN_RESET:
        eaten_today_reset();
        mqtt_eaten_today_sync();
        return PORT_OK;

    case MQTT_ROUTE_CMD_REBOOT:
        return reboot_request_arm();

    case MQTT_ROUTE_CMD_SCHEDULE_SET:
    case MQTT_ROUTE_CMD_SCHEDULE_DELETE:
    case MQTT_ROUTE_CMD_SCHEDULE_TOGGLE:
    case MQTT_ROUTE_CMD_SCHEDULE_SKIP:
    case MQTT_ROUTE_CMD_SCHEDULE_ENABLE:
    case MQTT_ROUTE_CMD_SCHEDULE_TODAY:
        if (body == NULL || len == 0u) {
            return PORT_ERR_INVALID_ARG;
        }

        if (!schedule_cmd_apply_json(route, body, len)) {
            return PORT_ERR_INVALID_ARG;
        }

        mqtt_schedule_request_publish();
        return PORT_OK;

    default:
        (void)device_id;
        return PORT_ERR_NOT_SUPPORTED;
    }
}
