/*
 * App state lock — spec/40-architecture/task-model.md § Shared app state
 *
 * The app task owns feeder state. Commands from the web server task run the
 * same handlers (app_cmd_dispatch); this mutex serialises the two so a web
 * calibration or display change never interleaves with an app event.
 */

#ifndef APP_LOCK_H
#define APP_LOCK_H

#include <stdbool.h>
#include <stdint.h>

void app_lock_init(void);
bool app_lock_take(uint32_t timeout_ms);
void app_lock_give(void);

#endif /* APP_LOCK_H */
