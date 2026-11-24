/*
 * Task watch — spec/30-processes/health-monitoring.md § Task watch
 *
 * Reports only: the user decides when to reboot.
 */

#include "task_watch.h"

#include <string.h>

#include "app_log.h"
#include "feed_history.h"
#include "problem.h"

typedef struct {
    volatile uint32_t beats;
    uint32_t seen;
    uint32_t last_change_ms;
    bool started;
    bool stalled;
} task_watch_t;

static task_watch_t s_watch[TASK_WATCH_COUNT];
static const char *const s_names[TASK_WATCH_COUNT] = { "app", "motor", "mqtt_io" };
static const char *s_reset_reason = "power";

void task_watch_beat(task_watch_id_t id)
{
    if ((unsigned)id < TASK_WATCH_COUNT) {
        s_watch[id].beats++;
    }
}

bool task_watch_check(task_watch_id_t id, uint32_t now_ms)
{
    task_watch_t *w;
    uint32_t beats;

    if ((unsigned)id >= TASK_WATCH_COUNT) {
        return false;
    }
    w = &s_watch[id];
    beats = w->beats;
    if (!w->started || beats != w->seen) {
        w->started = true;
        w->seen = beats;
        w->last_change_ms = now_ms;
        if (w->stalled) {
            w->stalled = false;
            app_log_info("watch", "task %s resumed", s_names[id]);
        }
        return false;
    }
    if (!w->stalled && (uint32_t)(now_ms - w->last_change_ms) >= TASK_WATCH_STALL_MS) {
        w->stalled = true;
        app_log_info("watch", "task %s stalled %lu s", s_names[id],
                     (unsigned long)((now_ms - w->last_change_ms) / 1000u));
        feed_counter_inc(FEED_COUNTER_STALLS);
        problem_raise(PROBLEM_TASK_STALL, s_names[id]);
    }
    return w->stalled;
}

void boot_info_set_reset_reason(const char *reason)
{
    if (reason != NULL) {
        s_reset_reason = reason;
    }
}

const char *boot_info_reset_reason(void)
{
    return s_reset_reason;
}

void task_watch_test_reset(void)
{
    memset(s_watch, 0, sizeof(s_watch));
    s_reset_reason = "power";
}
