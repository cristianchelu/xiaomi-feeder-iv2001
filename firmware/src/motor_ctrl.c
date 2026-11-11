/*
 * Motor control task — spec/30-processes/dispense-cycle.md,
 * spec/30-processes/jam-detection.md, spec/40-architecture/task-model.md
 */

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "app_log.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "adc_jam_isr_adapter.h"
#include "adc_port.h"
#include "app_event.h"
#include "app_log.h"
#include "gpio_expander_port.h"
#include "motor_ctrl.h"

#include <string.h>
#include "motor_driver.h"
#include "motor_index_adapter.h"
#include "motor_index_port.h"
#include "motor_jam.h"
#include "motor_limits.h"
#include "port_err.h"

typedef enum {
    MOTOR_CMD_BURST = 0,
    MOTOR_CMD_PARK,
    MOTOR_CMD_TIMED_FWD,
    MOTOR_CMD_TIMED_REV,
    MOTOR_CMD_STOP,
} motor_cmd_kind_t;

typedef struct {
    motor_cmd_kind_t kind;
    uint8_t pulse_target;
    uint16_t timeout_ms;
    uint8_t max_pulses;
} motor_cmd_t;

typedef enum {
    MOTOR_PHASE_IDLE = 0,
    MOTOR_PHASE_RUN_BURST,
    MOTOR_PHASE_RUN_PARK,
    MOTOR_PHASE_RUN_BENCH,
    MOTOR_PHASE_ANTIJAM_REV,
    MOTOR_PHASE_ANTIJAM_WIGGLE_FWD,
    MOTOR_PHASE_ANTIJAM_WIGGLE_REV,
} motor_phase_t;

typedef enum {
    MOTOR_JAM_NONE = 0,
    MOTOR_JAM_ADC_ISR,
    MOTOR_JAM_ADC_INSTANT,
    MOTOR_JAM_ADC_SUSTAINED,
    MOTOR_JAM_INDEX_TIMEOUT,
    MOTOR_JAM_SESSION_TIMEOUT,
    MOTOR_JAM_STALL,
} motor_jam_reason_t;

typedef enum {
    MOTOR_FAULT_ANTIJAM_EXHAUSTED = 0,
    MOTOR_FAULT_SESSION_TIMEOUT,
    MOTOR_FAULT_IO,
    MOTOR_FAULT_DRIVER_STOP,
    MOTOR_FAULT_DRIVER_START,
    MOTOR_FAULT_INDEX_LED,
} motor_fault_reason_t;

static motor_driver_state_t s_driver;
static bool s_driver_ready;
static SemaphoreHandle_t s_motor_mutex;
static QueueHandle_t s_cmd_q;
static TaskHandle_t s_task;
static motor_phase_t s_phase = MOTOR_PHASE_IDLE;
static motor_cmd_t s_active_cmd;
static volatile uint8_t s_pulses;
static bool s_prev_beam_open;
/* When the beam last closed: a hole only counts after a real closed stretch. */
static TickType_t s_beam_closed_tick;
/* Last counted pulse (or motor start): index timeout is per gap, not only 0 pulses. */
static TickType_t s_last_pulse_tick;
/* Forcing-without-turning timer (load above the stall level, burst only). */
static TickType_t s_stall_start_tick;
static bool s_stall_active;
/* Anti-jam resume continues the same burst: pulses already delivered count. */
static bool s_resume_keep_pulses;
/* Field trace: once a second while running, beam/poll/current summary. */
static uint16_t s_trace_ok;
static uint16_t s_trace_busy;
static uint16_t s_trace_err;
static TickType_t s_trace_tick;
static bool s_beam_closed_valid;
static TickType_t s_en_start_tick;
static TickType_t s_phase_end_tick;
static uint32_t s_sustained_over_ms;
static uint8_t s_antijam_retries;
static bool s_instant_jam;
static uint8_t s_io_fail_streak;
static TickType_t s_last_beam_poll_tick;
static TickType_t s_index_sample_after_tick;
static motor_jam_reason_t s_last_jam_reason;
static uint16_t s_jam_ma;
static uint32_t s_jam_sustained_ms;

static void motor_ctrl_session_teardown(void);
static void motor_ctrl_post_app_event(app_event_type_t type);

static TickType_t motor_ctrl_tick_now(void)
{
    return xTaskGetTickCount();
}

static bool motor_ctrl_tick_elapsed(TickType_t since, uint32_t ms)
{
    return (motor_ctrl_tick_now() - since) >= pdMS_TO_TICKS(ms);
}

static motor_run_stats_t s_stats;
static TickType_t s_stats_start_tick;

static void motor_ctrl_stats_begin(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.why = "-";
    s_stats.jam = "-";
    s_stats_start_tick = xTaskGetTickCount();
}

static const char *motor_ctrl_jam_short(motor_jam_reason_t reason)
{
    switch (reason) {
    case MOTOR_JAM_STALL:
        return "stall";
    case MOTOR_JAM_INDEX_TIMEOUT:
        return "index";
    case MOTOR_JAM_SESSION_TIMEOUT:
        return "session";
    case MOTOR_JAM_ADC_ISR:
    case MOTOR_JAM_ADC_INSTANT:
    case MOTOR_JAM_ADC_SUSTAINED:
        return "adc";
    default:
        return "-";
    }
}

static void motor_ctrl_stats_end(const char *why)
{
    s_stats.pulses = s_pulses;
    s_stats.antijam = s_antijam_retries;
    s_stats.why = why;
    s_stats.dur_ms = (uint32_t)((xTaskGetTickCount() - s_stats_start_tick) * portTICK_PERIOD_MS);
}

void motor_ctrl_last_run(motor_run_stats_t *out)
{
    if (out != NULL) {
        *out = s_stats;
        if (out->why == NULL) {
            out->why = "-";
        }
        if (out->jam == NULL) {
            out->jam = "-";
        }
    }
}

static const char *motor_ctrl_jam_reason_label(motor_jam_reason_t reason)
{
    switch (reason) {
    case MOTOR_JAM_ADC_ISR:
        return "adc isr";
    case MOTOR_JAM_ADC_INSTANT:
        return "adc instant";
    case MOTOR_JAM_ADC_SUSTAINED:
        return "adc sustained";
    case MOTOR_JAM_INDEX_TIMEOUT:
        return "index timeout";
    case MOTOR_JAM_SESSION_TIMEOUT:
        return "session timeout";
    case MOTOR_JAM_STALL:
        return "stall";
    default:
        return "unknown";
    }
}

static const char *motor_ctrl_cmd_label(void)
{
    return (s_active_cmd.kind == MOTOR_CMD_PARK) ? "park" : "burst";
}

static void motor_ctrl_log_jam(motor_jam_reason_t reason)
{
    s_last_jam_reason = reason;
    s_stats.jam = motor_ctrl_jam_short(reason);

    switch (reason) {
    case MOTOR_JAM_ADC_ISR:
        app_log_info("motor", "jam: adc isr (> %u mA)", MOTOR_JAM_INSTANT_MA);
        break;
    case MOTOR_JAM_ADC_INSTANT:
        app_log_info("motor", "jam: adc instant %u mA (> %u mA)",
                     (unsigned)s_jam_ma,
                     MOTOR_JAM_INSTANT_MA);
        break;
    case MOTOR_JAM_ADC_SUSTAINED:
        app_log_info("motor",
                     "jam: adc sustained %u mA for %lu ms (> %u mA)",
                     (unsigned)s_jam_ma,
                     (unsigned long)s_jam_sustained_ms,
                     MOTOR_JAM_SUSTAINED_MA);
        break;
    case MOTOR_JAM_STALL:
        app_log_info("motor", "jam: stall %u mA for %u ms without turning (>= %u mA)",
                     (unsigned)s_jam_ma, (unsigned)MOTOR_STALL_MS, (unsigned)MOTOR_STALL_MA);
        break;
    case MOTOR_JAM_INDEX_TIMEOUT:
        app_log_info("motor",
                     "jam: index timeout (no hole in %u ms, %u pulse(s), burst)",
                     (unsigned)s_active_cmd.timeout_ms, (unsigned)s_pulses);
        break;
    case MOTOR_JAM_SESSION_TIMEOUT:
        app_log_info("motor", "jam: session timeout (%u ms)",
                     (unsigned)MOTOR_RUN_MS_MAX);
        break;
    default:
        break;
    }
}

static void motor_ctrl_finish_fault(motor_fault_reason_t reason)
{
    static const char *const why[] = {
        [MOTOR_FAULT_ANTIJAM_EXHAUSTED] = "antijam",
        [MOTOR_FAULT_SESSION_TIMEOUT] = "session",
        [MOTOR_FAULT_IO] = "io",
        [MOTOR_FAULT_DRIVER_STOP] = "driver",
        [MOTOR_FAULT_DRIVER_START] = "driver",
        [MOTOR_FAULT_INDEX_LED] = "led",
    };

    motor_ctrl_stats_end(((size_t)reason < sizeof(why) / sizeof(why[0]) && why[reason] != NULL)
                             ? why[reason]
                             : "driver");
    switch (reason) {
    case MOTOR_FAULT_ANTIJAM_EXHAUSTED:
        app_log_info("motor",
                     "stuck: antijam retries exhausted (last jam: %s)",
                     motor_ctrl_jam_reason_label(s_last_jam_reason));
        break;
    case MOTOR_FAULT_SESSION_TIMEOUT:
        app_log_info("motor", "stuck: session timeout (%u ms)",
                     (unsigned)MOTOR_RUN_MS_MAX);
        break;
    case MOTOR_FAULT_IO:
        app_log_info("motor", "stuck: index I/O failed %u times",
                     (unsigned)s_io_fail_streak);
        break;
    case MOTOR_FAULT_DRIVER_STOP:
        app_log_info("motor", "stuck: motor stop failed");
        break;
    case MOTOR_FAULT_DRIVER_START:
        app_log_info("motor", "stuck: motor start failed");
        break;
    case MOTOR_FAULT_INDEX_LED:
        app_log_info("motor", "stuck: index LED on failed");
        break;
    default:
        app_log_info("motor", "stuck: fault");
        break;
    }

    motor_ctrl_session_teardown();
    motor_ctrl_post_app_event(EVT_MOTOR_FAULT);
}

static bool motor_ctrl_session_timed_out(TickType_t now)
{
    (void)now;

    if (s_phase == MOTOR_PHASE_IDLE) {
        return false;
    }

    /* A burst may legitimately need one index period per pulse (~6.4 s
     * measured); stalls are caught per gap by the index timeout. */
    /* The anti-jam phases of a burst share the burst limit: a late stall
     * must get its retries, not a session timeout at the first reverse. */
    if ((s_phase == MOTOR_PHASE_RUN_BURST || s_phase == MOTOR_PHASE_ANTIJAM_REV ||
         s_phase == MOTOR_PHASE_ANTIJAM_WIGGLE_FWD ||
         s_phase == MOTOR_PHASE_ANTIJAM_WIGGLE_REV) &&
        s_active_cmd.kind != MOTOR_CMD_PARK && s_active_cmd.timeout_ms > 0u) {
        uint32_t limit = (uint32_t)s_active_cmd.pulse_target * s_active_cmd.timeout_ms + 2000u;

        if (limit < MOTOR_RUN_MS_MAX) {
            limit = MOTOR_RUN_MS_MAX;
        }
        return motor_ctrl_tick_elapsed(s_en_start_tick, limit);
    }

    return motor_ctrl_tick_elapsed(s_en_start_tick, MOTOR_RUN_MS_MAX);
}

static bool motor_ctrl_force_stop(void)
{
    if (!motor_driver_is_running(&s_driver)) {
        return true;
    }

    return motor_driver_stop(&s_driver) == PORT_OK;
}

static void motor_hal_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void motor_ctrl_driver_ensure(void)
{
    if (s_driver_ready) {
        return;
    }

    motor_hw_t hw = {
        .expander = gpio_expander_port_get(),
        .delay_ms = motor_hal_delay_ms,
    };

    motor_driver_init(&s_driver, &hw);
    s_driver_ready = true;
}

static port_err_t motor_ctrl_mutex_take(void)
{
    if (s_motor_mutex == NULL) {
        s_motor_mutex = xSemaphoreCreateMutex();
        if (s_motor_mutex == NULL) {
            return PORT_ERR_IO;
        }
    }

    if (xSemaphoreTake(s_motor_mutex, 0) != pdPASS) {
        return PORT_ERR_BUSY;
    }

    return PORT_OK;
}

static void motor_ctrl_mutex_give(void)
{
    if (s_motor_mutex != NULL) {
        (void)xSemaphoreGive(s_motor_mutex);
    }
}

static void motor_ctrl_index_session_end(void)
{
    const motor_index_port_t *idx = motor_index_port_get();

    if (idx != NULL && idx->session_end != NULL) {
        (void)idx->session_end();
    }
}

static void motor_ctrl_post_app_event(app_event_type_t type)
{
    app_event_t ev;

    ev.type = type;
    if (!app_event_post(&ev)) {
        app_log_error("motor", "app event post failed type=%u", (unsigned)type);
    }
}

static void motor_ctrl_session_teardown(void)
{
    adc_jam_isr_adapter_stop();
    motor_ctrl_index_session_end();
    (void)motor_index_adapter_disarm_irq();
    (void)motor_ctrl_force_stop();
    s_io_fail_streak = 0u;
    s_last_beam_poll_tick = 0u;
    s_phase = MOTOR_PHASE_IDLE;
    motor_ctrl_mutex_give();
}

static void motor_ctrl_finish_burst_done(void)
{
    app_log_info("motor", "burst done: %u pulse(s) in %lu ms",
                 (unsigned)s_pulses,
                 (unsigned long)((motor_ctrl_tick_now() - s_en_start_tick) * portTICK_PERIOD_MS));
    if (!motor_ctrl_force_stop()) {
        motor_ctrl_finish_fault(MOTOR_FAULT_DRIVER_STOP);
        return;
    }

    motor_ctrl_stats_end("-");
    motor_ctrl_session_teardown();
    motor_ctrl_post_app_event(EVT_BURST_DONE);
}

static void motor_ctrl_finish_park_done(void)
{
    if (!motor_ctrl_force_stop()) {
        motor_ctrl_finish_fault(MOTOR_FAULT_DRIVER_STOP);
        return;
    }

    motor_ctrl_session_teardown();
    motor_ctrl_post_app_event(EVT_PARK_DONE);
}

static void motor_ctrl_finish_bench_done(void)
{
    if (!motor_ctrl_force_stop()) {
        motor_ctrl_finish_fault(MOTOR_FAULT_DRIVER_STOP);
        return;
    }

    motor_ctrl_session_teardown();
    motor_ctrl_post_app_event(EVT_TIMED_RUN_DONE);
}

static void motor_ctrl_note_io_failure(void)
{
    if (s_io_fail_streak < 255u) {
        s_io_fail_streak++;
    }

    if (s_io_fail_streak >= 3u) {
        motor_ctrl_finish_fault(MOTOR_FAULT_IO);
    }
}

static void motor_ctrl_note_io_success(void)
{
    s_io_fail_streak = 0u;
}

static port_err_t motor_ctrl_poll_beam_open(bool *beam_open)
{
    const motor_index_port_t *idx = motor_index_port_get();

    if (idx == NULL || idx->poll == NULL || beam_open == NULL) {
        return PORT_ERR_IO;
    }

    return idx->poll(beam_open);
}

static void motor_ctrl_finish_park_already_aligned(void)
{
    app_log_info("motor", "park: already aligned (beam open)");
    motor_ctrl_mutex_give();
    motor_ctrl_post_app_event(EVT_PARK_DONE);
}

static void motor_ctrl_schedule_index_sample(TickType_t now)
{
    TickType_t ready_at = now + pdMS_TO_TICKS(MOTOR_INDEX_IRQ_DEBOUNCE_MS);

    if (ready_at > s_index_sample_after_tick) {
        s_index_sample_after_tick = ready_at;
    }
}

static void motor_ctrl_apply_beam_sample(bool beam_open, TickType_t now)
{
    if (beam_open != s_prev_beam_open) {
        app_log_info("motor", "beam %s +%lu ms",
                     beam_open ? "open" : "closed",
                     (unsigned long)((now - s_en_start_tick) * portTICK_PERIOD_MS));
    }

    if (!beam_open && s_prev_beam_open) {
        s_beam_closed_tick = now;
        s_beam_closed_valid = true;
    }

    /* Count a hole only after the beam stayed closed for a real stretch
     * between holes. The auger parks at the hole edge, and the start-up jolt
     * makes the beam flicker closed→open: that used to be counted as a whole
     * portion, so a 1-portion burst (10 g, the dispense button) stopped after a
     * twitch and delivered nothing. */
    if (beam_open && !s_prev_beam_open) {
        if (s_beam_closed_valid &&
            (now - s_beam_closed_tick) >= pdMS_TO_TICKS(MOTOR_INDEX_MIN_CLOSED_MS)) {
            s_pulses++;
            s_last_pulse_tick = now;
            app_log_info("motor", "pulse %u", (unsigned)s_pulses);
        } else {
            app_log_info("motor", "edge ignored (closed %lu ms)",
                         s_beam_closed_valid
                             ? (unsigned long)((now - s_beam_closed_tick) * portTICK_PERIOD_MS)
                             : 0ul);
        }
        s_beam_closed_valid = false;
    }

    s_prev_beam_open = beam_open;

    if (s_phase == MOTOR_PHASE_RUN_PARK && beam_open) {
        motor_ctrl_finish_park_done();
    }
}

static void motor_ctrl_try_index_sample(TickType_t now)
{
    bool beam_open;
    port_err_t err;

    if (s_index_sample_after_tick == 0u || now < s_index_sample_after_tick) {
        return;
    }

    err = motor_ctrl_poll_beam_open(&beam_open);
    if (err == PORT_ERR_BUSY) {
        s_index_sample_after_tick = now + pdMS_TO_TICKS(MOTOR_CTRL_LOOP_SLICE_MS);
        return;
    }

    s_index_sample_after_tick = 0u;

    if (err != PORT_OK) {
        motor_ctrl_note_io_failure();
        return;
    }

    motor_ctrl_note_io_success();
    motor_ctrl_apply_beam_sample(beam_open, now);

    if (s_phase == MOTOR_PHASE_RUN_BURST &&
        s_pulses >= s_active_cmd.pulse_target) {
        motor_ctrl_finish_burst_done();
    }
}

static bool motor_ctrl_adc_sample(uint16_t *ma_out)
{
    const adc_port_t *adc = adc_port_get();
    uint16_t ma = 0u;

    if (adc == NULL || adc->try_read_motor_load_ma == NULL) {
        return false;
    }

    if (adc->try_read_motor_load_ma(&ma) != PORT_OK) {
        return false;
    }

    if (ma_out != NULL) {
        *ma_out = ma;
    }

    return true;
}

static bool motor_ctrl_is_jam_load(uint16_t ma)
{
    return ma > MOTOR_JAM_SUSTAINED_MA;
}

static motor_jam_reason_t motor_ctrl_detect_jam(TickType_t now)
{
    uint16_t ma = 0u;

    (void)now;

    if (s_instant_jam) {
        return MOTOR_JAM_ADC_ISR;
    }

    if (motor_ctrl_adc_sample(&ma)) {
        s_jam_ma = ma;
        if (ma > s_stats.max_ma) {
            s_stats.max_ma = ma;
        }

        if (ma > MOTOR_JAM_INSTANT_MA) {
            return MOTOR_JAM_ADC_INSTANT;
        }

        /* Field trace: a stalled auger draws ~280 mA without moving; normal
         * rotation is 70-150 mA. Back off after 2 s instead of waiting for the
         * 8 s index timeout. */
        if (s_phase == MOTOR_PHASE_RUN_BURST && ma >= MOTOR_STALL_MA &&
            ma <= MOTOR_JAM_SUSTAINED_MA) {
            if (!s_stall_active) {
                s_stall_active = true;
                s_stall_start_tick = motor_ctrl_tick_now();
            } else if (motor_ctrl_tick_elapsed(s_stall_start_tick, MOTOR_STALL_MS)) {
                s_stall_active = false;
                return MOTOR_JAM_STALL;
            }
        } else {
            s_stall_active = false;
        }

        if (ma > MOTOR_JAM_SUSTAINED_MA) {
            s_sustained_over_ms += MOTOR_CTRL_LOOP_SLICE_MS;
            if (s_sustained_over_ms >= MOTOR_JAM_SUSTAINED_MS) {
                s_jam_sustained_ms = s_sustained_over_ms;
                return MOTOR_JAM_ADC_SUSTAINED;
            }
        } else {
            s_sustained_over_ms = 0u;
        }
    }

    /* No new hole within the timeout since the last pulse (or start): the
     * auger stalled. Checking only "0 pulses" let a stall after the first
     * pulse run until the session limit with nothing delivered. */
    if (s_phase == MOTOR_PHASE_RUN_BURST &&
        s_active_cmd.timeout_ms > 0u &&
        motor_ctrl_tick_elapsed(s_last_pulse_tick, s_active_cmd.timeout_ms)) {
        return MOTOR_JAM_INDEX_TIMEOUT;
    }

    return MOTOR_JAM_NONE;
}

static void motor_ctrl_begin_antijam(motor_jam_reason_t reason)
{
    (void)motor_driver_stop(&s_driver);
    s_sustained_over_ms = 0u;
    s_instant_jam = false;

    if (s_antijam_retries >= MOTOR_ANTI_JAM_MAX_RETRIES) {
        motor_ctrl_finish_fault(MOTOR_FAULT_ANTIJAM_EXHAUSTED);
        return;
    }

    motor_ctrl_log_jam(reason);
    s_antijam_retries++;
    app_log_info("motor", "antijam: retry %u/%u reverse %u ms",
                 (unsigned)s_antijam_retries,
                 (unsigned)MOTOR_ANTI_JAM_MAX_RETRIES,
                 (unsigned)MOTOR_ANTI_JAM_REVERSE_MS);
    s_phase = MOTOR_PHASE_ANTIJAM_REV;
    (void)motor_driver_start_reverse(&s_driver);
    s_phase_end_tick = motor_ctrl_tick_now() + pdMS_TO_TICKS(MOTOR_ANTI_JAM_REVERSE_MS);
}

static port_err_t motor_ctrl_begin_run(bool park)
{
    port_err_t err;

    motor_ctrl_driver_ensure();
    if (!s_resume_keep_pulses) {
        s_pulses = 0u;
    }
    s_resume_keep_pulses = false;
    s_stall_active = false;
    /* The auger parks with the index hole in the beam (beam open). Assume that
     * until a read says otherwise, so a failed initial read never turns the
     * parked hole into a counted edge (a 1-portion burst stopped at once). */
    s_prev_beam_open = true;
    s_beam_closed_valid = false;
    s_beam_closed_tick = 0u;
    s_trace_ok = 0u;
    s_trace_busy = 0u;
    s_trace_err = 0u;
    s_trace_tick = motor_ctrl_tick_now();
    s_sustained_over_ms = 0u;
    s_instant_jam = false;
    s_io_fail_streak = 0u;
    s_last_beam_poll_tick = 0u;
    s_index_sample_after_tick = 0u;
    s_en_start_tick = motor_ctrl_tick_now();
    s_last_pulse_tick = s_en_start_tick;

    {
        const motor_index_port_t *idx = motor_index_port_get();

        if (idx == NULL || idx->session_begin == NULL ||
            idx->session_begin() != PORT_OK) {
            motor_ctrl_finish_fault(MOTOR_FAULT_INDEX_LED);
            return PORT_ERR_IO;
        }

        if (idx->poll != NULL) {
            bool beam_open = true;
            bool known = false;
            uint8_t tries;

            /* Baseline read: let the IR LED settle, retry a busy expander,
             * and only then fall back to the parked assumption above. */
            motor_hal_delay_ms(MOTOR_INDEX_LED_SETTLE_MS);
            for (tries = 0u; tries < MOTOR_INDEX_BASELINE_TRIES && !known; tries++) {
                if (idx->poll(&beam_open) == PORT_OK) {
                    known = true;
                } else {
                    motor_hal_delay_ms(MOTOR_INDEX_BASELINE_RETRY_MS);
                }
            }
            app_log_info("motor", "%s start: baseline %s (target %u)",
                         park ? "park" : "burst",
                         known ? (beam_open ? "open" : "closed") : "unread",
                         (unsigned)(park ? s_active_cmd.max_pulses : s_active_cmd.pulse_target));
            if (known) {
                s_prev_beam_open = beam_open;
                if (!beam_open) {
                    /* Closed at start: time it from now. The auger may have
                     * sprung back just out of the hole it stopped in. */
                    s_beam_closed_tick = motor_ctrl_tick_now();
                    s_beam_closed_valid = true;
                }
            } else {
                app_log_info("motor", "index baseline unread: assuming parked (beam open)");
            }
        }
    }

    if (s_task != NULL) {
        (void)motor_index_adapter_arm_irq(s_task, MOTOR_CTRL_NOTIFY_INDEX);
        adc_jam_isr_adapter_start(s_task, MOTOR_CTRL_NOTIFY_ADC_JAM);
    }

    err = motor_driver_start_forward(&s_driver);
    if (err != PORT_OK) {
        motor_ctrl_finish_fault(MOTOR_FAULT_DRIVER_START);
        return err;
    }

    s_phase = park ? MOTOR_PHASE_RUN_PARK : MOTOR_PHASE_RUN_BURST;
    return PORT_OK;
}

static port_err_t motor_ctrl_begin_bench_run(bool reverse)
{
    port_err_t err;

    motor_ctrl_driver_ensure();

    err = reverse ? motor_driver_start_reverse(&s_driver)
                  : motor_driver_start_forward(&s_driver);
    if (err != PORT_OK) {
        motor_ctrl_finish_fault(MOTOR_FAULT_DRIVER_START);
        return err;
    }

    s_phase = MOTOR_PHASE_RUN_BENCH;
    s_phase_end_tick = motor_ctrl_tick_now() +
                       pdMS_TO_TICKS(s_active_cmd.timeout_ms);
    return PORT_OK;
}

static void motor_ctrl_poll_bench(TickType_t now)
{
    if ((int32_t)(now - s_phase_end_tick) < 0) {
        return;
    }

    motor_ctrl_finish_bench_done();
}

static void motor_ctrl_retry_active_cmd(void)
{
    /* Same job after an anti-jam reverse: holes already counted stay counted,
     * otherwise every jam added extra portions. */
    s_resume_keep_pulses = true;
    if (s_active_cmd.kind == MOTOR_CMD_PARK) {
        (void)motor_ctrl_begin_run(true);
    } else {
        (void)motor_ctrl_begin_run(false);
    }
}

static void motor_ctrl_after_antijam_step(TickType_t now)
{
    uint16_t ma = 0u;
    bool still_jammed = false;

    (void)now;
    (void)motor_driver_stop(&s_driver);

    if (motor_ctrl_adc_sample(&ma) && motor_ctrl_is_jam_load(ma)) {
        still_jammed = true;
    }

    if (!still_jammed) {
        app_log_info("motor", "antijam: load %u mA ok, resuming %s",
                     (unsigned)ma,
                     motor_ctrl_cmd_label());
        motor_ctrl_retry_active_cmd();
        return;
    }

    if (s_phase == MOTOR_PHASE_ANTIJAM_REV) {
        app_log_info("motor",
                     "antijam: load %u mA still high, wiggle forward %u ms",
                     (unsigned)ma,
                     (unsigned)MOTOR_ANTI_JAM_WIGGLE_MS);
        s_phase = MOTOR_PHASE_ANTIJAM_WIGGLE_FWD;
        (void)motor_driver_start_forward(&s_driver);
        s_phase_end_tick = motor_ctrl_tick_now() + pdMS_TO_TICKS(MOTOR_ANTI_JAM_WIGGLE_MS);
        return;
    }

    if (s_phase == MOTOR_PHASE_ANTIJAM_WIGGLE_FWD) {
        app_log_info("motor",
                     "antijam: load %u mA still high, wiggle reverse %u ms",
                     (unsigned)ma,
                     (unsigned)MOTOR_ANTI_JAM_WIGGLE_MS);
        s_phase = MOTOR_PHASE_ANTIJAM_WIGGLE_REV;
        (void)motor_driver_start_reverse(&s_driver);
        s_phase_end_tick = motor_ctrl_tick_now() + pdMS_TO_TICKS(MOTOR_ANTI_JAM_WIGGLE_MS);
        return;
    }

    motor_ctrl_begin_antijam(s_last_jam_reason);
}

static void motor_ctrl_trace(TickType_t now, bool beam_open)
{
    uint16_t ma = 0u;

    if ((now - s_trace_tick) < pdMS_TO_TICKS(1000u)) {
        return;
    }
    s_trace_tick = now;
    (void)motor_ctrl_adc_sample(&ma);
    app_log_debug("motor", "run +%lu ms beam %s polls ok %u busy %u err %u load %u mA",
                 (unsigned long)((now - s_en_start_tick) * portTICK_PERIOD_MS),
                 beam_open ? "open" : "closed",
                 (unsigned)s_trace_ok, (unsigned)s_trace_busy, (unsigned)s_trace_err,
                 (unsigned)ma);
    s_trace_ok = 0u;
    s_trace_busy = 0u;
    s_trace_err = 0u;
}

static void motor_ctrl_poll_session_beam(TickType_t now, uint32_t interval_ms)
{
    bool beam_open;
    port_err_t err;

    if (s_last_beam_poll_tick != 0u &&
        (now - s_last_beam_poll_tick) < pdMS_TO_TICKS(interval_ms)) {
        return;
    }

    s_last_beam_poll_tick = now;

    err = motor_ctrl_poll_beam_open(&beam_open);
    if (err == PORT_ERR_BUSY) {
        s_trace_busy++;
        s_stats.poll_busy++;
        motor_ctrl_trace(now, s_prev_beam_open);
        return;
    }

    if (err != PORT_OK) {
        s_trace_err++;
        s_stats.poll_err++;
        motor_ctrl_trace(now, s_prev_beam_open);
        motor_ctrl_note_io_failure();
        return;
    }

    s_trace_ok++;
    s_stats.poll_ok++;
    motor_ctrl_trace(now, beam_open);

    motor_ctrl_note_io_success();
    motor_ctrl_apply_beam_sample(beam_open, now);

    if (s_phase == MOTOR_PHASE_IDLE) {
        return;
    }

    if (s_phase == MOTOR_PHASE_RUN_BURST &&
        s_pulses >= s_active_cmd.pulse_target) {
        motor_ctrl_finish_burst_done();
        return;
    }

    if (s_phase == MOTOR_PHASE_RUN_PARK &&
        s_pulses >= s_active_cmd.max_pulses) {
        motor_ctrl_finish_park_done();
    }
}

static void motor_ctrl_poll_run(TickType_t now)
{
    uint32_t notify = 0u;

    if (motor_ctrl_session_timed_out(now)) {
        motor_ctrl_finish_fault(MOTOR_FAULT_SESSION_TIMEOUT);
        return;
    }

    {
        motor_jam_reason_t jam = motor_ctrl_detect_jam(now);

        if (jam != MOTOR_JAM_NONE) {
            motor_ctrl_begin_antijam(jam);
            return;
        }
    }

    if (s_phase == MOTOR_PHASE_RUN_BURST &&
        s_pulses >= s_active_cmd.pulse_target) {
        motor_ctrl_finish_burst_done();
        return;
    }

    motor_ctrl_try_index_sample(now);

    if (s_phase == MOTOR_PHASE_RUN_BURST) {
        motor_ctrl_poll_session_beam(now, MOTOR_BURST_BEAM_POLL_MS);
        if (s_phase == MOTOR_PHASE_IDLE) {
            return;
        }
    }

    if (s_phase == MOTOR_PHASE_RUN_PARK) {
        motor_ctrl_poll_session_beam(now, MOTOR_PARK_BEAM_POLL_MS);
        if (s_phase == MOTOR_PHASE_IDLE) {
            return;
        }
    }

    (void)xTaskNotifyWait(0u, UINT32_MAX, &notify,
                          pdMS_TO_TICKS(MOTOR_CTRL_LOOP_SLICE_MS));

    if ((notify & MOTOR_CTRL_NOTIFY_INDEX) != 0u) {
        motor_ctrl_schedule_index_sample(now);
    }

    if ((notify & MOTOR_CTRL_NOTIFY_ADC_JAM) != 0u) {
        s_instant_jam = true;
    }
}

static void motor_ctrl_poll_antijam(TickType_t now)
{
    TickType_t remain;

    if (motor_ctrl_session_timed_out(now)) {
        motor_ctrl_finish_fault(MOTOR_FAULT_SESSION_TIMEOUT);
        return;
    }

    if ((int32_t)(now - s_phase_end_tick) < 0) {
        remain = s_phase_end_tick - now;
        if (remain > pdMS_TO_TICKS(MOTOR_CTRL_LOOP_SLICE_MS)) {
            remain = pdMS_TO_TICKS(MOTOR_CTRL_LOOP_SLICE_MS);
        }
        vTaskDelay(remain);
        return;
    }

    motor_ctrl_after_antijam_step(now);
}

static void motor_ctrl_handle_cmd(const motor_cmd_t *cmd)
{
    port_err_t err;

    if (cmd == NULL) {
        return;
    }

    if (cmd->kind == MOTOR_CMD_STOP) {
        if (s_phase != MOTOR_PHASE_IDLE) {
            s_instant_jam = false;
            motor_ctrl_session_teardown();
        }
        return;
    }

    err = motor_ctrl_mutex_take();
    if (err != PORT_OK) {
        (void)xQueueSendToFront(s_cmd_q, cmd, 0);
        return;
    }

    s_active_cmd = *cmd;
    s_antijam_retries = 0u;
    motor_ctrl_stats_begin();

    if (cmd->kind == MOTOR_CMD_PARK) {
        const motor_index_port_t *idx = motor_index_port_get();
        bool beam_open = false;

        if (idx != NULL && idx->sense != NULL &&
            idx->sense(&beam_open) == PORT_OK && beam_open) {
            motor_ctrl_finish_park_already_aligned();
            return;
        }

        (void)motor_ctrl_begin_run(true);
    } else if (cmd->kind == MOTOR_CMD_TIMED_FWD) {
        (void)motor_ctrl_begin_bench_run(false);
    } else if (cmd->kind == MOTOR_CMD_TIMED_REV) {
        (void)motor_ctrl_begin_bench_run(true);
    } else {
        (void)motor_ctrl_begin_run(false);
    }
}

static void motor_ctrl_task(void *param)
{
    motor_cmd_t cmd;

    (void)param;

    for (;;) {
        if (xQueueReceive(s_cmd_q, &cmd, pdMS_TO_TICKS(MOTOR_CTRL_LOOP_SLICE_MS)) ==
            pdPASS) {
            motor_ctrl_handle_cmd(&cmd);
        }

        if (s_phase == MOTOR_PHASE_IDLE) {
            continue;
        }

        {
            TickType_t now = motor_ctrl_tick_now();

            if (s_phase == MOTOR_PHASE_RUN_BURST ||
                s_phase == MOTOR_PHASE_RUN_PARK) {
                motor_ctrl_poll_run(now);
            } else if (s_phase == MOTOR_PHASE_RUN_BENCH) {
                motor_ctrl_poll_bench(now);
            } else {
                motor_ctrl_poll_antijam(now);
            }
        }
    }
}

void motor_ctrl_start(void)
{
    if (s_cmd_q == NULL) {
        s_cmd_q = xQueueCreate(MOTOR_CTRL_CMD_QUEUE_DEPTH, sizeof(motor_cmd_t));
    }

    if (s_task == NULL) {
        (void)xTaskCreate(motor_ctrl_task,
                          MOTOR_CTRL_TASK_NAME,
                          MOTOR_CTRL_TASK_STACK_BYTES / sizeof(portSTACK_TYPE),
                          NULL,
                          MOTOR_CTRL_TASK_PRIO,
                          &s_task);
    }
}

bool motor_ctrl_is_active(void)
{
    return s_phase != MOTOR_PHASE_IDLE;
}

static port_err_t motor_ctrl_enqueue(const motor_cmd_t *cmd)
{
    if (cmd == NULL || s_cmd_q == NULL) {
        return PORT_ERR_IO;
    }

    if (xQueueSend(s_cmd_q, cmd, 0) != pdPASS) {
        return PORT_ERR_BUSY;
    }

    return PORT_OK;
}

port_err_t motor_ctrl_request_burst(uint8_t pulse_target, uint16_t timeout_ms)
{
    motor_cmd_t cmd = {
        .kind = MOTOR_CMD_BURST,
        .pulse_target = pulse_target,
        .timeout_ms = timeout_ms,
        .max_pulses = 0u,
    };

    if (pulse_target == 0u || timeout_ms == 0u) {
        return PORT_ERR_INVALID_ARG;
    }

    if (motor_ctrl_is_active()) {
        return PORT_ERR_BUSY;
    }

    return motor_ctrl_enqueue(&cmd);
}

port_err_t motor_ctrl_request_park(uint8_t max_pulses)
{
    motor_cmd_t cmd = {
        .kind = MOTOR_CMD_PARK,
        .pulse_target = 0u,
        .timeout_ms = 0u,
        .max_pulses = max_pulses,
    };

    if (max_pulses == 0u) {
        return PORT_ERR_INVALID_ARG;
    }

    if (motor_ctrl_is_active()) {
        return PORT_ERR_BUSY;
    }

    return motor_ctrl_enqueue(&cmd);
}

static port_err_t motor_ctrl_request_timed_run(bool reverse, uint32_t duration_ms)
{
    motor_cmd_t cmd = {
        .kind = reverse ? MOTOR_CMD_TIMED_REV : MOTOR_CMD_TIMED_FWD,
        .pulse_target = 0u,
        .timeout_ms = (uint16_t)duration_ms,
        .max_pulses = 0u,
    };

    if (duration_ms == 0u || duration_ms > MOTOR_RUN_MS_MAX) {
        return PORT_ERR_INVALID_ARG;
    }

    if (motor_ctrl_is_active()) {
        return PORT_ERR_BUSY;
    }

    return motor_ctrl_enqueue(&cmd);
}

port_err_t motor_ctrl_request_timed_forward_ms(uint32_t duration_ms)
{
    return motor_ctrl_request_timed_run(false, duration_ms);
}

port_err_t motor_ctrl_request_timed_reverse_ms(uint32_t duration_ms)
{
    return motor_ctrl_request_timed_run(true, duration_ms);
}

port_err_t motor_ctrl_request_stop(void)
{
    motor_cmd_t cmd = {
        .kind = MOTOR_CMD_STOP,
    };

    return motor_ctrl_enqueue(&cmd);
}

void motor_ctrl_test_reset(void)
{
    motor_cmd_t drain;

    s_phase = MOTOR_PHASE_IDLE;
    s_antijam_retries = 0u;
    s_instant_jam = false;
    s_sustained_over_ms = 0u;
    s_pulses = 0u;

    if (s_cmd_q != NULL) {
        while (xQueueReceive(s_cmd_q, &drain, 0) == pdPASS) {
        }
    }

    if (s_motor_mutex != NULL) {
        (void)xSemaphoreTake(s_motor_mutex, 0);
        (void)xSemaphoreGive(s_motor_mutex);
    }

    motor_ctrl_driver_ensure();
    (void)motor_driver_stop(&s_driver);
    motor_ctrl_index_session_end();
}

void motor_ctrl_test_notify(uint32_t bits)
{
    if (s_task != NULL) {
        (void)xTaskNotify(s_task, bits, eSetBits);
    }
}

void motor_ctrl_test_poll(void)
{
    motor_cmd_t cmd;

    if (s_cmd_q != NULL &&
        xQueueReceive(s_cmd_q, &cmd, 0) == pdPASS) {
        motor_ctrl_handle_cmd(&cmd);
    }

    if (s_phase == MOTOR_PHASE_IDLE) {
        return;
    }

    {
        TickType_t now = motor_ctrl_tick_now();

        if (s_phase == MOTOR_PHASE_RUN_BURST ||
            s_phase == MOTOR_PHASE_RUN_PARK) {
            motor_ctrl_poll_run(now);
        } else if (s_phase == MOTOR_PHASE_RUN_BENCH) {
            motor_ctrl_poll_bench(now);
        } else {
            motor_ctrl_poll_antijam(now);
        }
    }
}

uint8_t motor_ctrl_pulses(void)
{
    return s_pulses;
}
