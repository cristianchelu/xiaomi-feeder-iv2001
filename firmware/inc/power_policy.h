/*
 * Power policy — Wi-Fi on battery, low-battery conservation, sleep.
 * spec/30-processes/power-state-machine.md
 */

#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BATTERY_WIFI_ON = 0,
    BATTERY_WIFI_OFF,
    BATTERY_WIFI_SCHEDULED_ONLY,
} battery_wifi_t;

/* scheduled_only: Wi-Fi up this many minutes before a scheduled dispense. */
#define POWER_POLICY_SCHEDULE_LEAD_MIN   2
/* ... and kept up this long after a dispense so the result is published. */
#define POWER_POLICY_DISPENSE_HOLD_MS    120000u
/* Pack percentage under which the device conserves energy on battery. */
#define POWER_POLICY_LOW_BATTERY_PCT     15u
#define POWER_POLICY_LOW_BATTERY_SCREEN_S 10u

const char *power_policy_battery_wifi_name(battery_wifi_t mode);
bool power_policy_battery_wifi_parse(const char *name, battery_wifi_t *out);

typedef struct {
    bool on_battery;
    bool asleep;
    bool low_battery;           /* pack below POWER_POLICY_LOW_BATTERY_PCT */
    battery_wifi_t mode;
    int32_t next_in_min;        /* minutes to next scheduled dispense, -1 unknown/none */
    bool dispense_recent;       /* dispense active or finished within the hold window */
} power_policy_input_t;

/* Should the Wi-Fi radio be connected right now? */
bool power_policy_wifi_wanted(const power_policy_input_t *in);

/* Effective screen-off delay on battery (0 = never). */
uint16_t power_policy_screen_off_s(uint16_t configured_s, bool on_battery, bool low_battery);

#endif /* POWER_POLICY_H */
