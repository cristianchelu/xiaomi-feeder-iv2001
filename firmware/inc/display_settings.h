/*
 * Persisted display settings — spec/30-processes/config-store.md (display group)
 */

#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DISPLAY_MODE_WEIGHT = 0,
    DISPLAY_MODE_EATEN_TODAY,
} display_mode_t;

/* 0 = panel off; 1..4 = lit. */
#define DISPLAY_BRIGHTNESS_MIN      0u
#define DISPLAY_BRIGHTNESS_MAX      4u
#define DISPLAY_BRIGHTNESS_DEFAULT  4u

#define DISPLAY_BATTERY_OFF_S_MAX      600u
#define DISPLAY_BATTERY_OFF_S_DEFAULT  60u

uint16_t display_settings_battery_off_s_get(void);
bool display_settings_battery_off_s_set(uint16_t seconds);

display_mode_t display_settings_mode_get(void);
bool display_settings_mode_set(display_mode_t mode);
uint8_t display_settings_brightness_get(void);
bool display_settings_brightness_set(uint8_t level);

const char *display_settings_mode_name(display_mode_t mode);
bool display_settings_mode_parse(const char *name, display_mode_t *out);

#endif /* DISPLAY_SETTINGS_H */
