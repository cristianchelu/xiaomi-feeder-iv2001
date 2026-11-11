/*
 * Persisted display settings — spec/30-processes/config-store.md (display group)
 */

#include "display_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_keys.h"
#include "config_port.h"

const char *display_settings_mode_name(display_mode_t mode)
{
    switch (mode) {
    case DISPLAY_MODE_EATEN_TODAY:
        return "eaten_today";
    default:
        return "weight";
    }
}

bool display_settings_mode_parse(const char *name, display_mode_t *out)
{
    if (name == NULL || out == NULL) {
        return false;
    }
    if (strcmp(name, "weight") == 0) {
        *out = DISPLAY_MODE_WEIGHT;
        return true;
    }
    if (strcmp(name, "eaten_today") == 0) {
        *out = DISPLAY_MODE_EATEN_TODAY;
        return true;
    }
    return false;
}

display_mode_t display_settings_mode_get(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[16];
    display_mode_t mode;

    if (cfg == NULL ||
        cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE, buf, sizeof(buf)) != PORT_OK ||
        !display_settings_mode_parse(buf, &mode)) {
        return DISPLAY_MODE_WEIGHT;
    }
    return mode;
}

bool display_settings_mode_set(display_mode_t mode)
{
    const config_port_t *cfg = config_port_get();

    if (cfg == NULL) {
        return false;
    }
    return cfg->write(CONFIG_GROUP_DISPLAY,
                      CONFIG_KEY_DISPLAY_MODE,
                      display_settings_mode_name(mode)) == PORT_OK;
}

uint8_t display_settings_brightness_get(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];
    char *end = NULL;
    long v;

    if (cfg != NULL) {
        char mode[16];

        /* Legacy firmware stored "off" as a display mode: now brightness 0. */
        if (cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE, mode, sizeof(mode)) == PORT_OK &&
            strcmp(mode, "off") == 0) {
            return 0u;
        }
    }
    if (cfg == NULL ||
        cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BRIGHTNESS, buf, sizeof(buf)) != PORT_OK) {
        return DISPLAY_BRIGHTNESS_DEFAULT;
    }
    v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || v < (long)DISPLAY_BRIGHTNESS_MIN ||
        v > (long)DISPLAY_BRIGHTNESS_MAX) {
        return DISPLAY_BRIGHTNESS_DEFAULT;
    }
    return (uint8_t)v;
}

bool display_settings_brightness_set(uint8_t level)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];

    if (cfg == NULL || level > DISPLAY_BRIGHTNESS_MAX) {
        return false;
    }
    /* Setting brightness clears a legacy "off" mode. */
    {
        char mode[16];

        if (cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE, mode, sizeof(mode)) == PORT_OK &&
            strcmp(mode, "off") == 0) {
            (void)cfg->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE, "weight");
        }
    }
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned)level);
    return cfg->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BRIGHTNESS, buf) == PORT_OK;
}

uint16_t display_settings_battery_off_s_get(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];
    char *end = NULL;
    long v;

    if (cfg == NULL ||
        cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BATTERY_OFF_S, buf, sizeof(buf)) != PORT_OK) {
        return DISPLAY_BATTERY_OFF_S_DEFAULT;
    }
    v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || v < 0 || v > (long)DISPLAY_BATTERY_OFF_S_MAX) {
        return DISPLAY_BATTERY_OFF_S_DEFAULT;
    }
    return (uint16_t)v;
}

bool display_settings_battery_off_s_set(uint16_t seconds)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];

    if (cfg == NULL || seconds > DISPLAY_BATTERY_OFF_S_MAX) {
        return false;
    }
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned)seconds);
    return cfg->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BATTERY_OFF_S, buf) == PORT_OK;
}

uint16_t display_settings_mains_off_s_get(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];
    char *end = NULL;
    long v;

    if (cfg == NULL ||
        cfg->read(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MAINS_OFF_S, buf, sizeof(buf)) != PORT_OK) {
        return DISPLAY_MAINS_OFF_S_DEFAULT;
    }
    v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || v < 0 || v > (long)DISPLAY_MAINS_OFF_S_MAX) {
        return DISPLAY_MAINS_OFF_S_DEFAULT;
    }
    return (uint16_t)v;
}

bool display_settings_mains_off_s_set(uint16_t seconds)
{
    const config_port_t *cfg = config_port_get();
    char buf[8];

    if (cfg == NULL || seconds > DISPLAY_MAINS_OFF_S_MAX) {
        return false;
    }
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned)seconds);
    return cfg->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MAINS_OFF_S, buf) == PORT_OK;
}
