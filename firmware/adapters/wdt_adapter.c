/*
 * Hardware watchdog — spec/30-processes/power-state-machine.md § Watchdog
 */

#include <stdbool.h>

#include "hal_wdt.h"

#include "app_log.h"
#include "wdt_adapter.h"

#define WDT_TIMEOUT_S 30u  /* [tune] HAL maximum */

static hal_wdt_reset_status_t s_reset_status = HAL_WDT_NONE_RESET;
static bool s_armed;

void wdt_adapter_arm_early(void)
{
    hal_wdt_config_t cfg;

    s_reset_status = hal_wdt_get_reset_status();

    cfg.mode = HAL_WDT_MODE_RESET;
    cfg.seconds = WDT_TIMEOUT_S;
    if (hal_wdt_init(&cfg) == HAL_WDT_STATUS_OK &&
        hal_wdt_enable(HAL_WDT_ENABLE_MAGIC) == HAL_WDT_STATUS_OK) {
        s_armed = true;
    }
}

void wdt_adapter_log_reset_reason(void)
{
    const char *reason = "power";

    if (s_reset_status == HAL_WDT_TIMEOUT_RESET) {
        reason = "watchdog";
    } else if (s_reset_status == HAL_WDT_SOFTWARE_RESET) {
        reason = "software";
    }
    APP_LOG_I("app", "reset reason: %s%s", reason, s_armed ? "" : " (watchdog NOT armed)");
}

void wdt_adapter_feed(void)
{
    (void)hal_wdt_feed(HAL_WDT_FEED_MAGIC);
}
