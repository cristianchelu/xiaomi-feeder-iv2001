/*
 * Motor jam / burst tuning — spec/30-processes/jam-detection.md,
 * spec/30-processes/dispense-cycle.md, spec/30-processes/motor-index.md
 */

#ifndef MOTOR_JAM_H
#define MOTOR_JAM_H

#include <stdint.h>

#define MOTOR_JAM_INSTANT_MA           1800u
#define MOTOR_JAM_SUSTAINED_MA          500u
#define MOTOR_JAM_SUSTAINED_MS         4000u
#define MOTOR_BURST_TIMEOUT_MS         8000u
#define MOTOR_BURST_PULSE_DEFAULT       1u
#define MOTOR_PARK_MAX_PULSES_DEFAULT   4u
#define MOTOR_ANTI_JAM_REVERSE_MS      1000u
#define MOTOR_ANTI_JAM_WIGGLE_MS        500u
#define MOTOR_ANTI_JAM_MAX_RETRIES        3u
#define MOTOR_CTRL_CMD_QUEUE_DEPTH        4u
#define MOTOR_CTRL_LOOP_SLICE_MS           10u
#define MOTOR_INDEX_IRQ_DEBOUNCE_MS        50u
#define MOTOR_BURST_BEAM_POLL_MS           50u
/* Burst/park start: IR LED settle before the baseline beam read, and retries
 * while the GPIO expander is busy (weight / hopper reads share it). */
#define MOTOR_INDEX_LED_SETTLE_MS          5u
#define MOTOR_INDEX_BASELINE_TRIES         5u
#define MOTOR_INDEX_BASELINE_RETRY_MS      10u
/* Minimum closed stretch before a hole counts as an index pulse, measured on
 * the device (mains): one hole every ~6.4 s, beam open ~0.2 s. When the motor
 * stops, the loaded auger springs back out of the hole; on the next start it
 * re-enters it within ~160 ms. Counting from motor start (a closed baseline is
 * not "already closed"), anything under 1.5 s is that spring-back, not a turn. */
#define MOTOR_INDEX_MIN_CLOSED_MS       1500u
#define MOTOR_PARK_BEAM_POLL_MS           200u
/* Stall: load at/above this for this long during a burst = forcing without
 * turning (field: ~280 mA stalled vs 70-150 mA turning). */
#define MOTOR_STALL_MA                    220u
#define MOTOR_STALL_MS                   2000u

#endif /* MOTOR_JAM_H */
