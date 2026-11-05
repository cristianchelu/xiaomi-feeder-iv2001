/*
 * System port adapter — MT7682 warm reboot.
 */

#include "system_port.h"

#include "FreeRTOS.h"
#include "task.h"
#include "hal_cache.h"
#include "hal_sys.h"

void system_port_reboot(void)
{
    vTaskDelay(200 / portTICK_PERIOD_MS);
    hal_cache_disable();
    hal_cache_deinit();
    hal_sys_reboot(HAL_SYS_REBOOT_MAGIC, WHOLE_SYSTEM_REBOOT_COMMAND);
}
