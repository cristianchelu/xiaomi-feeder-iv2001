/*
 * Health tick — spec/30-processes/health-monitoring.md
 * Live problems, connection-lost tracking, midnight clear, not-eaten poll,
 * task watch, problem publish and the front-panel problem code.
 */

#ifndef APP_HEALTH_H
#define APP_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#define APP_HEALTH_TIME_GRACE_MS   120000u   /* [tune] no E5 right after boot */
#define APP_HEALTH_MQTT_LOST_MS    300000u   /* [tune] E4 after 5 min down */

typedef struct {
    bool mqtt_up;
    bool mqtt_armed;
    bool panel_free;     /* no dispense / OTA / blank / sleep on the panel */
} app_health_inputs_t;

void app_health_tick(uint32_t now_ms, const app_health_inputs_t *in);

/* Segments for the problem code at now_ms: Err (1.5 s), E<n> (1.5 s), then
 * normal content (3 s). False = show normal content. */
bool app_health_problem_segments(int code, uint32_t now_ms, uint8_t out[3]);

void app_health_test_reset(void);

#endif /* APP_HEALTH_H */
