/*
 * Idle panel blank on battery — spec/30-processes/display-presentation.md
 */

#include "display_idle.h"

static uint32_t s_last_activity_ms;

void display_idle_reset(uint32_t now_ms)
{
    s_last_activity_ms = now_ms;
}

void display_idle_note_activity(uint32_t now_ms)
{
    s_last_activity_ms = now_ms;
}

bool display_idle_blank_wanted(uint32_t now_ms, bool on_battery, uint16_t timeout_s)
{
    if (!on_battery || timeout_s == 0u) {
        return false;
    }
    return (uint32_t)(now_ms - s_last_activity_ms) >= (uint32_t)timeout_s * 1000u;
}
