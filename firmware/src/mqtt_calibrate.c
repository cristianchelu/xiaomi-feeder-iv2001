/*
 * MQTT scale calibration command — spec/30-processes/mqtt-protocol.md § Calibrate command
 */

#include "mqtt_calibrate.h"

#include <string.h>

#include "app_log.h"
#include "dispense.h"
#include "feeder_runtime.h"
#include "mqtt_json.h"
#include "weight_port.h"

port_err_t mqtt_calibrate_handle(const void *payload, size_t len)
{
    const weight_port_t *wp;
    char action[8];
    port_err_t err;
    bool zero;

    if (payload == NULL || len == 0u ||
        !mqtt_json_find_string(payload, len, "action", action, sizeof(action))) {
        return PORT_ERR_INVALID_ARG;
    }

    if (strcmp(action, "zero") == 0) {
        zero = true;
    } else if (strcmp(action, "span") == 0) {
        zero = false;
    } else {
        return PORT_ERR_INVALID_ARG;
    }

    if (dispense_is_active() || feeder_runtime_ota_active()) {
        return PORT_ERR_BUSY;
    }

    wp = weight_port_get();
    if (wp == NULL || wp->calibrate_zero == NULL || wp->calibrate_span == NULL) {
        return PORT_ERR_NOT_SUPPORTED;
    }

    err = zero ? wp->calibrate_zero() : wp->calibrate_span();
    if (err != PORT_OK) {
        app_log_info("app", "calibrate %s failed (%s)", action, port_err_name(err));
        return err;
    }

    app_log_info("app", "calibrate %s ok", action);
    return PORT_OK;
}
