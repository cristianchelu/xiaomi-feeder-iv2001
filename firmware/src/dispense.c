/*
 * Dispense supervisor — spec/30-processes/dispense-cycle.md
 */

#include "dispense.h"

#include <stdio.h>
#include <string.h>
#include "motor_ctrl.h"
#include "mqtt_dispense_progress.h"
#include "mqtt_portion.h"
#include "portion_estimate.h"

#include "app.h"
#include "app_event.h"
#include "app_log.h"
#include "auto_tare.h"
#include "bowl_error.h"
#include "dispense_cli.h"
#include "eaten_today.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "mqtt_port.h"
#include "problem.h"
#include "time_sync.h"
#include "display_dispense_indicator.h"
#include "feed_config.h"
#include "feeder_runtime.h"
#include "FreeRTOS.h"
#include "hopper_input.h"
#include "hopper_level.h"
#include "motor_jam.h"
#include "motor_port.h"
#include "mqtt_client.h"
#include "mqtt_dispense_event.h"
#include "port_err.h"
#include "schedule.h"
#include "task.h"
#include "weight_port.h"
#include "weight_units.h"

typedef enum {
    DISPENSE_PHASE_NONE = 0,
    DISPENSE_PHASE_MOTOR,
    DISPENSE_PHASE_SETTLE,
} dispense_phase_t;

static bool s_job_pending;
static dispense_phase_t s_phase;
static dispense_outcome_t s_outcome;
static dispense_source_t s_source;
static uint8_t s_portions;
static uint8_t s_total_portions;
static uint8_t s_batch_portions;   /* portions of the batch being weighed */
static bool s_cal_requested;       /* next portions job is a portion calibration */
static bool s_portion_cal;         /* current job is a portion calibration */
static uint16_t s_target_grams;
static weight_dg_t s_target_dg;
static bool s_gram_job;
static dispense_mode_t s_mode;
static uint8_t s_batch_count;
static weight_dg_t s_baseline_dg;
static weight_dg_t s_last_settle_dg;
static bool s_baseline_valid;
static uint32_t s_settle_start_ms;
static uint8_t s_zero_delta_streak;
static motor_run_stats_t s_job_motor;   /* accumulated over the job's bursts */
static bool s_bowl_at_start;

static void dispense_accumulate_motor(void)
{
    motor_run_stats_t r;

    motor_ctrl_last_run(&r);
    s_job_motor.pulses = (uint8_t)(s_job_motor.pulses + r.pulses);
    s_job_motor.antijam = (uint8_t)(s_job_motor.antijam + r.antijam);
    s_job_motor.poll_ok = (uint16_t)(s_job_motor.poll_ok + r.poll_ok);
    s_job_motor.poll_busy = (uint16_t)(s_job_motor.poll_busy + r.poll_busy);
    s_job_motor.poll_err = (uint16_t)(s_job_motor.poll_err + r.poll_err);
    s_job_motor.dur_ms += r.dur_ms;
    if (r.max_ma > s_job_motor.max_ma) {
        s_job_motor.max_ma = r.max_ma;
    }
    if (r.why != NULL && strcmp(r.why, "-") != 0) {
        s_job_motor.why = r.why;
    }
    if (r.jam != NULL && strcmp(r.jam, "-") != 0) {
        s_job_motor.jam = r.jam;
    }
}

static void dispense_job_stats_begin(void)
{
    memset(&s_job_motor, 0, sizeof(s_job_motor));
    s_job_motor.why = "-";
    s_job_motor.jam = "-";
}

static char dispense_source_char(dispense_source_t src)
{
    switch (src) {
    case DISPENSE_SOURCE_SCHEDULE:
        return 's';
    case DISPENSE_SOURCE_BUTTON:
        return 'b';
    default:
        return 'm';
    }
}

static const char *dispense_outcome_label(dispense_outcome_t o)
{
    switch (o) {
    case DISPENSE_OUTCOME_SUCCESS:
        return "ok";
    case DISPENSE_OUTCOME_UNDERFILL:
        return "underfill";
    case DISPENSE_OUTCOME_STUCK:
        return "stuck";
    case DISPENSE_OUTCOME_EMPTY_HOPPER:
        return "empty_hopper";
    default:
        return "aborted";
    }
}

static void dispense_record_base(feed_record_t *r, dispense_source_t src, uint16_t want_g)
{
    const mqtt_port_t *mqtt = mqtt_port_get();
    int64_t epoch = 0;

    memset(r, 0, sizeof(*r));
    if (time_sync_get_utc_epoch(&epoch) && epoch > 0) {
        r->t = (uint32_t)epoch;
    }
    r->day = eaten_today_day_key_now();
    r->src = dispense_source_char(src);
    r->want = want_g;
    strcpy(r->why, "-");
    strcpy(r->jam, "-");
    r->mqtt = mqtt != NULL && mqtt->is_connected != NULL && mqtt->is_connected();
    r->bowl = true;
}

void dispense_record_refused(dispense_source_t src, uint16_t want_g)
{
    feed_record_t r;

    dispense_record_base(&r, src, want_g);
    strcpy(r.res, "limit");
    feed_history_add(&r);
    feed_counter_inc(FEED_COUNTER_FAILED);
    problem_raise(PROBLEM_DAILY_LIMIT, "limit reached");
    app_log_info("dispense", "daily limit reached: %u g refused", (unsigned)want_g);
    mqtt_feed_limit_sync();
}

/* Daily limit before a job (health-monitoring.md § Daily limit).
 * Returns false when refused; *portions_io is shortened when needed. */
static bool dispense_limit_gate(dispense_source_t src, uint16_t want_g, uint8_t *portions_io)
{
    uint32_t rem;
    weight_dg_t est = portion_estimate_dg();
    uint32_t want_dg = (uint32_t)*portions_io * (uint32_t)est;

    if (src == DISPENSE_SOURCE_UART || !feed_limit_remaining_g(&rem)) {
        return true;
    }
    if (rem == 0u) {
        dispense_record_refused(src, want_g);
        return false;
    }
    if (want_dg > (uint32_t)WEIGHT_G_TO_DG(rem)) {
        uint32_t p = (uint32_t)WEIGHT_G_TO_DG(rem) / (uint32_t)est;

        *portions_io = (uint8_t)(p < 1u ? 1u : p);
        problem_raise(PROBLEM_DAILY_LIMIT, "meal shortened");
        app_log_info("dispense", "daily limit: %lu g left, %u portion(s)",
                     (unsigned long)rem, (unsigned)*portions_io);
    }
    return true;
}

static void dispense_note_bowl_at_start(void)
{
    const weight_port_t *wp = weight_port_get();
    weight_cal_status_t cal = (wp != NULL && wp->get_cal_status != NULL) ? wp->get_cal_status()
                                                                       : WEIGHT_CAL_UNCALIBRATED;

    s_bowl_at_start = !(s_baseline_valid &&
                        bowl_error_eval(cal, true, s_baseline_dg) == BOWL_ERROR_BOWL_MISSING);
    if (!s_bowl_at_start) {
        problem_raise(PROBLEM_BOWL_MISSING, NULL);
        app_log_info("dispense", "bowl missing at start: dispensing anyway");
    }
}

/* History, counters, problems, learning, not-eaten (health-monitoring.md). */
static void dispense_after_job(uint32_t now_ms, int32_t grams, bool measured, int32_t raw_delta,
                               bool was_calibration)
{
    feed_record_t r;
    unsigned i;
    bool ok = s_outcome == DISPENSE_OUTCOME_SUCCESS || s_outcome == DISPENSE_OUTCOME_UNDERFILL;

    if (s_source == DISPENSE_SOURCE_UART) {
        return;
    }
    dispense_record_base(&r, s_source, s_gram_job ? s_target_grams : 0u);
    r.got = (int16_t)grams;
    r.meas = measured;
    (void)snprintf(r.res, sizeof(r.res), "%s", dispense_outcome_label(s_outcome));
    r.por = s_total_portions;
    r.pul = s_job_motor.pulses;
    r.aj = s_job_motor.antijam;
    (void)snprintf(r.why, sizeof(r.why), "%s", s_job_motor.why != NULL ? s_job_motor.why : "-");
    (void)snprintf(r.jam, sizeof(r.jam), "%s", s_job_motor.jam != NULL ? s_job_motor.jam : "-");
    r.ok = s_job_motor.poll_ok;
    r.busy = s_job_motor.poll_busy;
    r.err = s_job_motor.poll_err;
    r.ma = s_job_motor.max_ma;
    r.dur = (uint16_t)((s_job_motor.dur_ms + 500u) / 1000u);
    r.bowl = s_bowl_at_start;
    feed_history_add(&r);

    feed_counter_inc(ok ? FEED_COUNTER_OK : FEED_COUNTER_FAILED);
    for (i = 0u; i < s_job_motor.antijam; i++) {
        feed_counter_inc(FEED_COUNTER_JAMS);
    }

    if (!ok) {
        problem_raise(PROBLEM_FEED_FAILED, r.res);
    } else if (measured && raw_delta < 10) {
        problem_raise(PROBLEM_FEED_FAILED, "nothing fell");
    } else if (s_outcome == DISPENSE_OUTCOME_SUCCESS) {
        problem_clear(PROBLEM_FEED_FAILED);
    }

    if (!was_calibration && measured && s_outcome == DISPENSE_OUTCOME_SUCCESS && raw_delta > 0 &&
        portion_estimate_learn((weight_dg_t)raw_delta, s_total_portions)) {
        mqtt_portion_sync();
    }
    if (measured && raw_delta > 0) {
        feed_not_eaten_on_feed((weight_dg_t)raw_delta, now_ms);
    }
    mqtt_feed_limit_sync();
}

static void dispense_progress_publish(void)
{
    uint8_t before = (uint8_t)(s_total_portions - s_batch_portions);
    uint8_t done;

    switch (s_phase) {
    case DISPENSE_PHASE_MOTOR:
        done = (uint8_t)(before + motor_ctrl_pulses());
        if (done > s_total_portions) {
            done = s_total_portions;
        }
        mqtt_dispense_progress_update("running", done, s_total_portions,
                                      (weight_dg_t)done * portion_estimate_dg());
        break;
    case DISPENSE_PHASE_SETTLE:
        mqtt_dispense_progress_update("weighing", s_total_portions, s_total_portions,
                                      (weight_dg_t)s_total_portions * portion_estimate_dg());
        break;
    default:
        mqtt_dispense_progress_update("idle", 0u, 0u, 0);
        break;
    }
}

void dispense_progress_poll(void)
{
    if (s_phase == DISPENSE_PHASE_MOTOR) {
        dispense_progress_publish();
    }
}

static void dispense_sync_runtime_active(void)
{
    feeder_runtime_set_dispense_active(s_job_pending || s_phase != DISPENSE_PHASE_NONE);
    dispense_progress_publish();
}

static bool dispense_capture_baseline(uint32_t now_ms)
{
    if (auto_tare_pending_calibration()) {
        const weight_port_t *wp = weight_port_get();
        weight_dg_t dg;
        port_err_t err;

        if (wp == NULL || wp->read_dg == NULL) {
            s_baseline_valid = false;
            return false;
        }

        err = wp->read_dg(&dg);
        if (err != PORT_OK) {
            s_baseline_valid = false;
            return false;
        }

        auto_tare_anchor(dg);
        s_baseline_dg = dg;
        s_baseline_valid = true;
        app_bowl_dg_notify_read(dg, true, now_ms);
        return true;
    }

    app_bowl_dg_snapshot_t snap;

    if (app_bowl_dg_snapshot(now_ms, &snap) &&
        snap.valid &&
        snap.sample_age_ms < DISPENSE_BASELINE_FRESH_MS) {
        s_baseline_dg = snap.dg;
        s_baseline_valid = true;
        return true;
    }

    {
        const weight_port_t *wp = weight_port_get();
        weight_dg_t dg;
        port_err_t err;

        if (wp == NULL || wp->read_dg == NULL) {
            s_baseline_valid = false;
            return false;
        }

        err = wp->read_dg(&dg);
        if (err != PORT_OK) {
            s_baseline_valid = false;
            return false;
        }

        s_baseline_dg = dg;
        s_baseline_valid = true;
        app_bowl_dg_notify_read(dg, true, now_ms);
        return true;
    }
}

static bool dispense_read_post_dg(weight_dg_t *dg_out)
{
    const weight_port_t *wp = weight_port_get();
    port_err_t err;

    if (wp == NULL || wp->read_dg == NULL || dg_out == NULL) {
        return false;
    }

    err = wp->read_dg(dg_out);
    return err == PORT_OK;
}

static bool dispense_scale_trusted(weight_dg_t dg, bool valid)
{
    const weight_port_t *wp = weight_port_get();
    weight_cal_status_t cal;
    bowl_error_kind_t bowl_err;

    if (!valid) {
        return false;
    }

    cal = (wp != NULL && wp->get_cal_status != NULL) ? wp->get_cal_status()
                                                   : WEIGHT_CAL_UNCALIBRATED;
    bowl_err = bowl_error_eval(cal, true, dg);
    return !bowl_error_is_active(bowl_err);
}

static void dispense_update_zero_delta_streak(weight_dg_t raw_delta)
{
    if (raw_delta <= 0) {
        if (s_zero_delta_streak < 255u) {
            s_zero_delta_streak++;
        }
    } else {
        s_zero_delta_streak = 0u;
    }
}

static dispense_mode_t dispense_resolve_mode(const app_dispense_request_t *req)
{
    if (req != NULL &&
        req->kind == DISPENSE_KIND_PORTIONS &&
        req->source == DISPENSE_SOURCE_UART) {
        return DISPENSE_MODE_OPEN_LOOP;
    }

    return feed_config_mode_get();
}

static void dispense_notify_cli_outcome(dispense_outcome_t outcome)
{
    if (outcome == DISPENSE_OUTCOME_SUCCESS) {
        (void)dispense_cli_on_job_done();
    } else {
        (void)dispense_cli_on_job_fault(outcome);
    }
}

static void dispense_publish_terminal(uint32_t now_ms,
                                      int32_t event_grams,
                                      bool measured,
                                      int32_t raw_delta)
{
    dispense_completion_t completion;

    bool was_calibration = s_portion_cal;

    s_outcome = hopper_level_on_dispense_finished(s_outcome,
                                                  raw_delta,
                                                  measured,
                                                  now_ms);

    if (s_portion_cal) {
        if (measured && raw_delta > 0 && s_outcome == DISPENSE_OUTCOME_SUCCESS) {
            (void)portion_estimate_calibrate((weight_dg_t)raw_delta, s_total_portions);
        } else {
            app_log_info("dispense", "portion calibration failed (measured %d delta %ld outcome %d)",
                         (int)measured, (long)raw_delta, (int)s_outcome);
            portion_estimate_calibration_failed();
        }
        s_portion_cal = false;
        mqtt_portion_sync();
    }

    dispense_after_job(now_ms, event_grams, measured, raw_delta, was_calibration);

    completion.grams = event_grams;
    completion.grams_estimated = !measured;
    completion.target_g = s_target_grams;
    completion.outcome = s_outcome;
    completion.source = s_source;
    completion.mode = s_mode;
    completion.batch_count = s_batch_count;
    completion.portions = s_total_portions;
    completion.has_slot = schedule_active_slot(&completion.slot_hour, &completion.slot_min);

    if (s_source == DISPENSE_SOURCE_SCHEDULE && completion.has_slot) {
        schedule_dispense_result_t sched_result;

        sched_result.hour = completion.slot_hour;
        sched_result.min = completion.slot_min;
        sched_result.grams = completion.grams;

        switch (s_outcome) {
        case DISPENSE_OUTCOME_SUCCESS:
            sched_result.outcome = SCHEDULE_DISPENSE_OK;
            break;
        case DISPENSE_OUTCOME_UNDERFILL:
            sched_result.outcome = SCHEDULE_DISPENSE_UNDERFILL;
            break;
        default:
            sched_result.outcome = SCHEDULE_DISPENSE_FAILED;
            break;
        }

        schedule_on_dispense_complete(&sched_result);
    }

    {
        const char *device_id = mqtt_client_device_id();

        if (device_id != NULL && device_id[0] != '\0') {
            mqtt_dispense_event_set_device_id(device_id);
            (void)mqtt_dispense_event_publish(&completion);
        }
    }

    app_weight_notify_dispense_complete();

    s_phase = DISPENSE_PHASE_NONE;
    s_job_pending = false;
    s_gram_job = false;
    display_dispense_indicator_idle();

    dispense_notify_cli_outcome(s_outcome);
    hopper_level_notify_dispense_complete();
    dispense_sync_runtime_active();
}

static void dispense_begin_settle(dispense_outcome_t outcome)
{
    s_outcome = outcome;
    s_phase = DISPENSE_PHASE_SETTLE;
    s_settle_start_ms = 0u;
    dispense_sync_runtime_active();
}

static port_err_t dispense_kick_motor(uint8_t pulse_target)
{
    const motor_port_t *motor = motor_port_get();

    if (motor == NULL || motor->request_burst == NULL) {
        return PORT_ERR_IO;
    }

    return motor->request_burst(pulse_target, MOTOR_BURST_TIMEOUT_MS);
}

static void dispense_after_settle(uint32_t now_ms)
{
    weight_dg_t post_dg = 0;
    bool post_valid;
    weight_dg_t raw_delta_dg;
    weight_dg_t batch_delta_dg;
    weight_dg_t dg_delivered;
    int32_t event_grams;
    bool measured;

    if (s_phase == DISPENSE_PHASE_NONE) {
        return;
    }

    post_valid = dispense_read_post_dg(&post_dg);
    if (post_valid) {
        auto_tare_anchor(post_dg);
        app_bowl_dg_notify_read(post_dg, true, now_ms);
    }

    measured = s_baseline_valid && post_valid &&
               dispense_scale_trusted(s_baseline_dg, s_baseline_valid) &&
               dispense_scale_trusted(post_dg, post_valid);

    if (measured) {
        dg_delivered = post_dg - s_baseline_dg;
        batch_delta_dg = post_dg - s_last_settle_dg;
        dispense_update_zero_delta_streak(batch_delta_dg);
        s_last_settle_dg = post_dg;
        raw_delta_dg = dg_delivered;
        event_grams = WEIGHT_DG_TO_G_ROUND(dg_delivered);
        if (event_grams < 0) {
            app_log_info("dispense", "negative delta clamped raw=%ld",
                         (long)WEIGHT_DG_TO_G_ROUND(raw_delta_dg));
            event_grams = 0;
        }
    } else {
        if (s_baseline_valid && post_valid) {
            raw_delta_dg = post_dg - s_baseline_dg;
            batch_delta_dg = post_dg - s_last_settle_dg;
            dispense_update_zero_delta_streak(batch_delta_dg);
            s_last_settle_dg = post_dg;
        } else {
            raw_delta_dg = 0;
            batch_delta_dg = 0;
        }
        dg_delivered = (weight_dg_t)s_total_portions * portion_estimate_dg();
        event_grams = WEIGHT_DG_TO_G_ROUND(dg_delivered);
    }

    app_log_info("dispense", "settle: base %ld post %ld dg (valid %d/%d) measured %d portions %u batch %u",
                 (long)s_baseline_dg, (long)post_dg, (int)s_baseline_valid, (int)post_valid,
                 (int)measured, (unsigned)s_total_portions, (unsigned)s_batch_count);

    if (s_outcome == DISPENSE_OUTCOME_STUCK) {
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    if (s_mode == DISPENSE_MODE_OPEN_LOOP || !measured) {
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    if (dg_delivered >= s_target_dg) {
        s_outcome = DISPENSE_OUTCOME_SUCCESS;
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    if (dg_delivered >= s_target_dg - DISPENSE_COMP_TOLERANCE_DG) {
        s_outcome = DISPENSE_OUTCOME_SUCCESS;
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    if (s_batch_count >= DISPENSE_COMP_MAX_BATCHES) {
        s_outcome = DISPENSE_OUTCOME_UNDERFILL;
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    if (s_zero_delta_streak >= DISPENSE_COMP_ZERO_DELTA_GIVEUP) {
        s_outcome = DISPENSE_OUTCOME_UNDERFILL;
        dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
        return;
    }

    {
        weight_dg_t deficit_dg = s_target_dg - dg_delivered;
        uint8_t extra;
        port_err_t err;

        extra = portion_estimate_portions_for_dg(deficit_dg, DISPENSE_PORTIONS_MAX);
        if (extra < 1u) {
            extra = 1u;
        }
        if (extra > DISPENSE_PORTIONS_MAX) {
            extra = DISPENSE_PORTIONS_MAX;
        }

        err = dispense_kick_motor(extra);
        if (err != PORT_OK) {
            s_outcome = DISPENSE_OUTCOME_ABORTED;
            dispense_publish_terminal(now_ms, event_grams, measured, raw_delta_dg);
            return;
        }

        s_total_portions = (uint8_t)(s_total_portions + extra);
        s_batch_portions = extra;
        s_batch_count++;
        s_phase = DISPENSE_PHASE_MOTOR;
        s_settle_start_ms = 0u;
        dispense_sync_runtime_active();
    }
}

dispense_submit_result_t dispense_submit_portion_cal(dispense_source_t source)
{
    dispense_submit_result_t r;

    s_cal_requested = true;
    r = dispense_submit_portions(PORTION_CAL_PORTIONS, source);
    if (r != DISPENSE_SUBMIT_OK) {
        s_cal_requested = false;
    }
    return r;
}

dispense_submit_result_t dispense_submit_portions(uint8_t portions,
                                                  dispense_source_t source)
{
    app_event_t ev;

    if (portions < DISPENSE_PORTIONS_MIN || portions > DISPENSE_PORTIONS_MAX) {
        app_log_info("dispense", "rejected result=%d", (int)DISPENSE_SUBMIT_INVALID);
        return DISPENSE_SUBMIT_INVALID;
    }

    if (dispense_is_active()) {
        app_log_info("dispense", "busy");
        return DISPENSE_SUBMIT_BUSY;
    }

    {
        const motor_port_t *motor = motor_port_get();

        if (motor != NULL && motor->is_active != NULL && motor->is_active()) {
            app_log_info("dispense", "busy");
            return DISPENSE_SUBMIT_BUSY;
        }
    }

    ev.type = EVT_DISPENSE_REQUEST;
    ev.u.dispense_request.kind = DISPENSE_KIND_PORTIONS;
    ev.u.dispense_request.target = portions;
    ev.u.dispense_request.source = source;

    if (!app_event_post(&ev)) {
        app_log_info("dispense", "busy");
        return DISPENSE_SUBMIT_BUSY;
    }

    s_job_pending = true;
    dispense_sync_runtime_active();
    app_log_info("dispense", "started portions=%u", (unsigned)portions);
    return DISPENSE_SUBMIT_OK;
}

dispense_submit_result_t dispense_submit_grams(uint8_t grams,
                                               dispense_source_t source)
{
    app_event_t ev;

    if (grams < SCHEDULE_G_MIN || grams > SCHEDULE_G_MAX) {
        app_log_info("dispense", "rejected result=%d", (int)DISPENSE_SUBMIT_INVALID);
        return DISPENSE_SUBMIT_INVALID;
    }

    if (dispense_is_active()) {
        app_log_info("dispense", "busy");
        return DISPENSE_SUBMIT_BUSY;
    }

    {
        const motor_port_t *motor = motor_port_get();

        if (motor != NULL && motor->is_active != NULL && motor->is_active()) {
            app_log_info("dispense", "busy");
            return DISPENSE_SUBMIT_BUSY;
        }
    }

    ev.type = EVT_DISPENSE_REQUEST;
    ev.u.dispense_request.kind = DISPENSE_KIND_GRAMS;
    ev.u.dispense_request.target = grams;
    ev.u.dispense_request.source = source;

    if (!app_event_post(&ev)) {
        app_log_info("dispense", "busy");
        return DISPENSE_SUBMIT_BUSY;
    }

    s_job_pending = true;
    dispense_sync_runtime_active();
    app_log_info("dispense", "started grams=%u", (unsigned)grams);
    return DISPENSE_SUBMIT_OK;
}

bool dispense_is_active(void)
{
    return s_job_pending || s_phase != DISPENSE_PHASE_NONE;
}

void dispense_start_from_request(const app_dispense_request_t *req)
{
    uint8_t portions;
    port_err_t err;
    uint32_t now_ms;

    if (req == NULL || !s_job_pending || s_phase != DISPENSE_PHASE_NONE) {
        app_log_info("dispense", "busy");
        dispense_cli_cancel_wait();
        s_job_pending = false;
        dispense_sync_runtime_active();
        return;
    }

    if (req->kind == DISPENSE_KIND_GRAMS) {
        uint8_t grams = (uint8_t)req->target;
        uint8_t portions;

        if (grams < SCHEDULE_G_MIN || grams > SCHEDULE_G_MAX) {
            app_log_info("dispense", "busy");
            dispense_cli_cancel_wait();
            s_job_pending = false;
            dispense_sync_runtime_active();
            return;
        }

        portions = portion_estimate_portions_for_dg(WEIGHT_G_TO_DG(grams), DISPENSE_PORTIONS_MAX);
        if (portions < DISPENSE_PORTIONS_MIN) {
            portions = DISPENSE_PORTIONS_MIN;
        }
        if (portions > DISPENSE_PORTIONS_MAX) {
            portions = DISPENSE_PORTIONS_MAX;
        }
        if (!dispense_limit_gate(req->source, grams, &portions)) {
            dispense_cli_cancel_wait();
            s_job_pending = false;
            dispense_sync_runtime_active();
            return;
        }
        dispense_job_stats_begin();

        s_target_grams = grams;
        s_target_dg = WEIGHT_G_TO_DG(grams);
        s_gram_job = true;
        s_portions = portions;
        s_total_portions = portions;
        s_batch_portions = portions;
        s_source = req->source;
        s_mode = dispense_resolve_mode(req);
        s_batch_count = 1u;
        s_zero_delta_streak = 0u;
        s_job_pending = false;
        s_phase = DISPENSE_PHASE_MOTOR;
        dispense_sync_runtime_active();

        now_ms = (uint32_t)(xTaskGetTickCount() * (TickType_t)portTICK_PERIOD_MS);
        (void)dispense_capture_baseline(now_ms);
        s_last_settle_dg = s_baseline_valid ? s_baseline_dg : 0;
        dispense_note_bowl_at_start();

        if (display_dispense_indicator_active() != PORT_OK) {
            app_log_info("dispense", "indicator unavailable");
        }

        err = dispense_kick_motor(portions);
        if (err != PORT_OK) {
            s_phase = DISPENSE_PHASE_NONE;
            s_gram_job = false;
            display_dispense_indicator_idle();
            app_log_info("dispense", "busy");
            dispense_cli_cancel_wait();
            dispense_sync_runtime_active();
            return;
        }

        return;
    }

    if (req->kind != DISPENSE_KIND_PORTIONS) {
        app_log_info("dispense", "busy");
        dispense_cli_cancel_wait();
        s_job_pending = false;
        dispense_sync_runtime_active();
        return;
    }

    portions = (uint8_t)req->target;
    if (portions < DISPENSE_PORTIONS_MIN || portions > DISPENSE_PORTIONS_MAX) {
        app_log_info("dispense", "busy");
        dispense_cli_cancel_wait();
        s_job_pending = false;
        dispense_sync_runtime_active();
        return;
    }

    {
        uint16_t want_g = (uint16_t)WEIGHT_DG_TO_G_ROUND((weight_dg_t)portions * portion_estimate_dg());

        if (!s_cal_requested && !dispense_limit_gate(req->source, want_g, &portions)) {
            dispense_cli_cancel_wait();
            s_job_pending = false;
            dispense_sync_runtime_active();
            return;
        }
    }
    dispense_job_stats_begin();
    s_job_pending = false;
    s_gram_job = false;
    s_portions = portions;
    s_total_portions = portions;
    s_batch_portions = portions;
    s_target_dg = (weight_dg_t)portions * portion_estimate_dg();
    s_target_grams = (uint16_t)WEIGHT_DG_TO_G_ROUND(s_target_dg);
    s_source = req->source;
    s_mode = dispense_resolve_mode(req);
    s_portion_cal = s_cal_requested;
    s_cal_requested = false;
    if (s_portion_cal) {
        s_mode = DISPENSE_MODE_OPEN_LOOP;   /* weigh exactly the holes turned */
        app_log_info("dispense", "portion calibration: %u portions", (unsigned)portions);
    }
    s_batch_count = 1u;
    s_zero_delta_streak = 0u;
    s_phase = DISPENSE_PHASE_MOTOR;
    dispense_sync_runtime_active();

    now_ms = (uint32_t)(xTaskGetTickCount() * (TickType_t)portTICK_PERIOD_MS);
    (void)dispense_capture_baseline(now_ms);
    s_last_settle_dg = s_baseline_valid ? s_baseline_dg : 0;
    dispense_note_bowl_at_start();

    if (display_dispense_indicator_active() != PORT_OK) {
        app_log_info("dispense", "indicator unavailable");
    }

    err = dispense_kick_motor(portions);
    if (err != PORT_OK) {
        s_phase = DISPENSE_PHASE_NONE;
        display_dispense_indicator_idle();
        app_log_info("dispense", "busy");
        dispense_cli_cancel_wait();
        dispense_sync_runtime_active();
    }
}

bool dispense_on_burst_done(void)
{
    if (s_phase != DISPENSE_PHASE_MOTOR) {
        return false;
    }

    dispense_accumulate_motor();
    dispense_begin_settle(DISPENSE_OUTCOME_SUCCESS);
    return true;
}

bool dispense_on_motor_fault(void)
{
    if (s_phase != DISPENSE_PHASE_MOTOR) {
        return false;
    }

    dispense_accumulate_motor();
    dispense_begin_settle(DISPENSE_OUTCOME_STUCK);
    return true;
}

void dispense_poll(uint32_t now_ms)
{
    if (s_phase != DISPENSE_PHASE_SETTLE) {
        return;
    }

    if (s_settle_start_ms == 0u) {
        s_settle_start_ms = now_ms;
        return;
    }

    if ((now_ms - s_settle_start_ms) < DISPENSE_SETTLE_MS) {
        return;
    }

    dispense_after_settle(now_ms);
}

uint8_t dispense_test_zero_delta_streak(void)
{
    return s_zero_delta_streak;
}

void dispense_test_reset(void)
{
    portion_estimate_test_reset();
    s_cal_requested = false;
    s_portion_cal = false;
    s_job_pending = false;
    s_phase = DISPENSE_PHASE_NONE;
    s_outcome = DISPENSE_OUTCOME_SUCCESS;
    s_source = DISPENSE_SOURCE_MQTT;
    s_portions = 0u;
    s_total_portions = 0u;
    s_batch_portions = 0u;
    s_gram_job = false;
    s_mode = DISPENSE_MODE_OPEN_LOOP;
    s_batch_count = 0u;
    s_target_grams = 0u;
    s_target_dg = 0;
    s_baseline_dg = 0;
    s_last_settle_dg = 0;
    s_baseline_valid = false;
    s_settle_start_ms = 0u;
    s_zero_delta_streak = 0u;
    display_dispense_indicator_idle();
    dispense_sync_runtime_active();
}
