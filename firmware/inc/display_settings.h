/*
 * Persisted display settings — spec/30-processes/config-store.md (display group)
 */

#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DISPLAY_MODE_WEIGHT = 0,
    DISPLAY_MODE_OFF,
} display_mode_t;

#define DISPLAY_BRIGHTNESS_MIN      1u
#define DISPLAY_BRIGHTNESS_MAX      4u
#define DISPLAY_BRIGHTNESS_DEFAULT  4u

display_mode_t display_settings_mode_get(void);
bool display_settings_mode_set(display_mode_t mode);
uint8_t display_settings_brightness_get(void);
bool display_settings_brightness_set(uint8_t level);

const char *display_settings_mode_name(display_mode_t mode);
bool display_settings_mode_parse(const char *name, display_mode_t *out);

#endif /* DISPLAY_SETTINGS_H */
