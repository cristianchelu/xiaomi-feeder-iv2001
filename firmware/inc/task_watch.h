/*
 * Task watch — spec/30-processes/health-monitoring.md § Task watch
 */

#ifndef TASK_WATCH_H
#define TASK_WATCH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TASK_WATCH_APP = 0,
    TASK_WATCH_MOTOR,
    TASK_WATCH_MQTT,
    TASK_WATCH_COUNT
} task_watch_id_t;

#define TASK_WATCH_STALL_MS 60000u   /* [tune] */

/* Called by the task itself on every loop iteration. */
void task_watch_beat(task_watch_id_t id);
/* Called by another task: logs once and raises E9 after TASK_WATCH_STALL_MS
 * without a beat. Returns true while the task is stalled. */
bool task_watch_check(task_watch_id_t id, uint32_t now_ms);

/* Last reset reason ("power", "watchdog", "software"), set at boot. */
void boot_info_set_reset_reason(const char *reason);
const char *boot_info_reset_reason(void);

void task_watch_test_reset(void);

#endif /* TASK_WATCH_H */
