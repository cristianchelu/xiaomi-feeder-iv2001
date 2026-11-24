/*
 * Idle panel blank on battery — spec/30-processes/display-presentation.md
 */

#ifndef DISPLAY_IDLE_H
#define DISPLAY_IDLE_H

#include <stdbool.h>
#include <stdint.h>

void display_idle_reset(uint32_t now_ms);
void display_idle_note_activity(uint32_t now_ms);
/* True when the panel should be blanked for inactivity. */
bool display_idle_blank_wanted(uint32_t now_ms, bool on_battery, uint16_t timeout_s);

#endif /* DISPLAY_IDLE_H */
