/*
 * Power policy — spec/30-processes/power-state-machine.md
 */

#include "power_policy.h"

#include <string.h>

const char *power_policy_battery_wifi_name(battery_wifi_t mode)
{
    switch (mode) {
    case BATTERY_WIFI_OFF:
        return "off";
    case BATTERY_WIFI_SCHEDULED_ONLY:
        return "scheduled_only";
    default:
        return "on";
    }
}

bool power_policy_battery_wifi_parse(const char *name, battery_wifi_t *out)
{
    if (name == NULL || out == NULL) {
        return false;
    }
    if (strcmp(name, "on") == 0) {
        *out = BATTERY_WIFI_ON;
    } else if (strcmp(name, "off") == 0) {
        *out = BATTERY_WIFI_OFF;
    } else if (strcmp(name, "scheduled_only") == 0) {
        *out = BATTERY_WIFI_SCHEDULED_ONLY;
    } else {
        return false;
    }
    return true;
}

bool power_policy_wifi_wanted(const power_policy_input_t *in)
{
    battery_wifi_t mode;

    if (in == NULL || in->asleep) {
        return false;
    }
    if (!in->on_battery) {
        return true;
    }

    mode = in->mode;
    if (in->low_battery && mode == BATTERY_WIFI_ON) {
        mode = BATTERY_WIFI_SCHEDULED_ONLY;
    }

    switch (mode) {
    case BATTERY_WIFI_OFF:
        return false;
    case BATTERY_WIFI_SCHEDULED_ONLY:
        return in->dispense_recent ||
               (in->next_in_min >= 0 && in->next_in_min <= POWER_POLICY_SCHEDULE_LEAD_MIN);
    default:
        return true;
    }
}

uint16_t power_policy_screen_off_s(uint16_t configured_s, bool on_battery, bool low_battery)
{
    if (!on_battery || !low_battery) {
        return configured_s;
    }
    if (configured_s == 0u || configured_s > POWER_POLICY_LOW_BATTERY_SCREEN_S) {
        return POWER_POLICY_LOW_BATTERY_SCREEN_S;
    }
    return configured_s;
}
