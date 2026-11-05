/*
 * Battery ADC sampling and MQTT sync — spec/30-processes/battery-monitoring.md
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

bool battery_monitor_poll(uint32_t now_ms);
void battery_monitor_force_sample(void);
/* Last sampled pack percentage, or BATTERY_MONITOR_PCT_UNKNOWN. */
#define BATTERY_MONITOR_PCT_UNKNOWN  255u
#define BATTERY_MONITOR_LOW_PCT      15u
uint8_t battery_monitor_pct(void);

void battery_monitor_test_reset(void);

#endif /* BATTERY_MONITOR_H */
