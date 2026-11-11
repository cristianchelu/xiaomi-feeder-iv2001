/*
 * Motor control task — spec/40-architecture/task-model.md,
 * spec/30-processes/dispense-cycle.md
 */

#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include <stdbool.h>
#include <stdint.h>

#include "port_err.h"

#define MOTOR_CTRL_NOTIFY_INDEX   (1u << 0)
#define MOTOR_CTRL_NOTIFY_ADC_JAM (1u << 1)

/* Owned constants — spec/40-architecture/task-model.md (SDK TASK_PRIORITY_HIGH = 6). */
#define MOTOR_CTRL_TASK_NAME         "motor_ctrl"
#define MOTOR_CTRL_TASK_STACK_BYTES  2048u
#define MOTOR_CTRL_TASK_PRIO         6u

void motor_ctrl_start(void);
bool motor_ctrl_is_active(void);

port_err_t motor_ctrl_request_burst(uint8_t pulse_target, uint16_t timeout_ms);
port_err_t motor_ctrl_request_park(uint8_t max_pulses);
/* Index pulses counted so far in the current burst (progress display). */
uint8_t motor_ctrl_pulses(void);
port_err_t motor_ctrl_request_timed_forward_ms(uint32_t duration_ms);
port_err_t motor_ctrl_request_timed_reverse_ms(uint32_t duration_ms);
port_err_t motor_ctrl_request_stop(void);

/* Diagnostics of the last (or current) burst session, for the feed history
 * (spec/30-processes/health-monitoring.md § Feed history). */
typedef struct {
    uint8_t pulses;
    uint8_t antijam;
    const char *why;   /* "-", "antijam", "session", "io", "driver", "led" */
    const char *jam;   /* "-", "stall", "index", "adc", "session" */
    uint16_t poll_ok;
    uint16_t poll_busy;
    uint16_t poll_err;
    uint16_t max_ma;
    uint32_t dur_ms;
} motor_run_stats_t;

void motor_ctrl_last_run(motor_run_stats_t *out);

#endif /* MOTOR_CTRL_H */
