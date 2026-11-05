/*
 * Deferred device reboot — spec/30-processes/mqtt-protocol.md § Reboot command
 */

#include "reboot_request.h"

#include "app_log.h"
#include "dispense.h"
#include "feeder_runtime.h"
#include "system_port.h"

static bool s_pending;
static bool s_deadline_set;
static uint32_t s_deadline_ms;

port_err_t reboot_request_arm(void)
{
    if (dispense_is_active() || feeder_runtime_ota_active()) {
        return PORT_ERR_BUSY;
    }
    s_pending = true;
    return PORT_OK;
}

bool reboot_request_pending(void)
{
    return s_pending;
}

void reboot_request_poll(uint32_t now_ms)
{
    if (!s_pending) {
        return;
    }
    if (!s_deadline_set) {
        s_deadline_ms = now_ms + REBOOT_REQUEST_DELAY_MS;
        s_deadline_set = true;
        app_log_info("system", "reboot requested");
        return;
    }
    if ((int32_t)(now_ms - s_deadline_ms) >= 0) {
        s_pending = false;
        s_deadline_set = false;
        system_port_reboot();
    }
}

void reboot_request_test_reset(void)
{
    s_pending = false;
    s_deadline_set = false;
    s_deadline_ms = 0u;
}
