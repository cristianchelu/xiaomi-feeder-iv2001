/*
 * App state lock — spec/40-architecture/task-model.md § Shared app state
 */

#include "app_lock.h"

#ifdef HOST_TEST

void app_lock_init(void) {}
bool app_lock_take(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return true;
}
void app_lock_give(void) {}

#else

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_lock;

void app_lock_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }
}

bool app_lock_take(uint32_t timeout_ms)
{
    if (s_lock == NULL) {
        return true;   /* before init (boot): single-threaded */
    }
    return xSemaphoreTake(s_lock, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void app_lock_give(void)
{
    if (s_lock != NULL) {
        (void)xSemaphoreGive(s_lock);
    }
}

#endif
