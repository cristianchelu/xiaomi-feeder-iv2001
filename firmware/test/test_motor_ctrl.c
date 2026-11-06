/* Tests: spec/30-processes/dispense-cycle.md, spec/30-processes/motor-index.md,
 * spec/30-processes/jam-detection.md */

#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "app_event.h"
#include "app_event_port.h"
#include "board_gpio_iv2001.h"
#include "fake_adc_port.h"
#include "fake_gpio_expander_port.h"
#include "fake_wfci_bus_port.h"
#include "fake_motor_index_port.h"
#include "fake_time.h"
#include "motor_ctrl.h"
#include "motor_ctrl_test.h"
#include "motor_index_port_provider_host.h"
#include "motor_port_provider_host.h"
#include "motor_jam.h"
#include "motor_limits.h"

extern void fake_app_event_q_reset(void);
extern void freertos_notify_test_reset(void);

static void motor_ctrl_test_setup(void)
{
    motor_index_port_host_reset();
    motor_port_host_reset();
    fake_gpio_expander_reset();
    fake_wfci_bus_reset();
    fake_motor_index_port_reset();
    fake_adc_port_reset();
    fake_time_reset();
    freertos_notify_test_reset();
    fake_app_event_q_reset();
    app_event_port_init();
    motor_ctrl_start();
    motor_ctrl_test_reset();
}

static void motor_ctrl_run_until_idle(uint32_t max_polls)
{
    uint32_t i;

    for (i = 0u; i < max_polls; i++) {
        motor_ctrl_test_poll();
        if (!motor_ctrl_is_active()) {
            break;
        }
    }
}

static void capture_stdout_begin(FILE **saved, FILE **cap)
{
    *saved = stdout;
    *cap = tmpfile();
    TEST_ASSERT_NOT_NULL(*cap);
    stdout = *cap;
}

static void capture_stdout_end(FILE *saved, FILE *cap, char *buf, size_t len)
{
    size_t n = 0u;
    int ch;

    fflush(stdout);
    rewind(cap);
    if (buf != NULL && len > 0u) {
        while (n + 1u < len && (ch = fgetc(cap)) != EOF) {
            buf[n++] = (char)ch;
        }
        buf[n] = '\0';
    }
    stdout = saved;
    fclose(cap);
}

static bool app_event_pending_type(app_event_type_t expect)
{
    app_event_t ev;

    if (!app_event_try_receive(&ev)) {
        return false;
    }

    TEST_ASSERT_EQUAL(expect, ev.type);
    app_event_release(&ev);
    return true;
}

void test_motor_ctrl_park_ends_on_beam_open(void)
{
    motor_ctrl_test_setup();
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_park(MOTOR_PARK_MAX_PULSES_DEFAULT));

    motor_ctrl_test_poll();
    fake_motor_index_port_set_beam_open(true);
    fake_time_advance_ms(MOTOR_PARK_BEAM_POLL_MS + 1u);
    motor_ctrl_run_until_idle(50u);

    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_PARK_DONE));
}

void test_motor_ctrl_park_done_immediately_when_already_open(void)
{
    char log[128];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(true);
    capture_stdout_begin(&saved, &cap);
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_park(MOTOR_PARK_MAX_PULSES_DEFAULT));
    motor_ctrl_test_poll();
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    TEST_ASSERT_FALSE(fake_gpio_expander_pin(BOARD_GPIO_MOTOR_EN_PORT,
                                             BOARD_GPIO_MOTOR_EN_PIN));
    TEST_ASSERT_FALSE(fake_motor_index_port_get_led());
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_PARK_DONE));
    TEST_ASSERT_NOT_NULL(strstr(log, "park: already aligned (beam open)"));
}

void test_motor_ctrl_session_faults_on_20s_cap_when_stop_fails(void)
{
    motor_ctrl_test_setup();
  /* 1=reset stop, 2=PH, 3=EN on start; stop on session end is call 4+. */
    fake_gpio_expander_set_set_pin_fail_range(4u, 0u, PORT_ERR_IO);

    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    fake_time_advance_ms(MOTOR_RUN_MS_MAX + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_run_until_idle(200u);

    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_MOTOR_FAULT));
}

void test_motor_ctrl_burst_faults_after_index_timeout_retries(void)
{
    char log[16384];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    capture_stdout_begin(&saved, &cap);
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));

    motor_ctrl_test_poll();
    fake_time_advance_ms(MOTOR_BURST_TIMEOUT_MS + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_run_until_idle(5000u);
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_MOTOR_FAULT));
    TEST_ASSERT_NOT_NULL(strstr(log, "jam: index timeout"));
    TEST_ASSERT_NOT_NULL(strstr(log, "antijam: retry"));
    TEST_ASSERT_NOT_NULL(
        strstr(log, "stuck: antijam retries exhausted (last jam: index timeout)"));
}

void test_motor_ctrl_logs_sustained_adc_jam(void)
{
    char log[1024];
    FILE *saved;
    FILE *cap;
    uint32_t i;

    motor_ctrl_test_setup();
    fake_adc_port_set_motor_ma(600u);
    capture_stdout_begin(&saved, &cap);
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();

    for (i = 0u; i < (MOTOR_JAM_SUSTAINED_MS / MOTOR_CTRL_LOOP_SLICE_MS) + 1u;
         i++) {
        motor_ctrl_test_poll();
        if (!motor_ctrl_is_active()) {
            break;
        }
    }
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_NOT_NULL(strstr(log, "jam: adc sustained 600 mA"));
    TEST_ASSERT_NOT_NULL(strstr(log, "antijam: retry 1/3"));
}

void test_motor_ctrl_session_fault_logs_timeout(void)
{
    char log[512];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    fake_gpio_expander_set_set_pin_fail_range(4u, 0u, PORT_ERR_IO);
    capture_stdout_begin(&saved, &cap);
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    fake_time_advance_ms(MOTOR_RUN_MS_MAX + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_run_until_idle(200u);
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_NOT_NULL(strstr(log, "stuck: session timeout (20000 ms)"));
}

void test_motor_ctrl_timed_forward_posts_done(void)
{
    motor_ctrl_test_setup();
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_timed_forward_ms(500u));
    motor_ctrl_test_poll();
    TEST_ASSERT_TRUE(motor_ctrl_is_active());
    fake_time_advance_ms(500u + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_run_until_idle(50u);
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_TIMED_RUN_DONE));
}

void test_motor_ctrl_timed_reverse_posts_done(void)
{
    motor_ctrl_test_setup();
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_timed_reverse_ms(300u));
    motor_ctrl_test_poll();
    TEST_ASSERT_TRUE(motor_ctrl_is_active());
    fake_time_advance_ms(300u + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_run_until_idle(50u);
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_TIMED_RUN_DONE));
}

void test_motor_ctrl_timed_run_rejects_when_active(void)
{
    motor_ctrl_test_setup();
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    TEST_ASSERT_EQUAL(PORT_ERR_BUSY, motor_ctrl_request_timed_forward_ms(100u));
}

void test_motor_ctrl_park_survives_burst_index_timeout(void)
{
    motor_ctrl_test_setup();
    TEST_ASSERT_EQUAL(PORT_OK,
                      motor_ctrl_request_park(MOTOR_PARK_MAX_PULSES_DEFAULT));

    motor_ctrl_test_poll();
    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    fake_time_advance_ms(MOTOR_BURST_TIMEOUT_MS + MOTOR_CTRL_LOOP_SLICE_MS);
    motor_ctrl_test_poll();

    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    fake_motor_index_port_set_beam_open(true);
    fake_time_advance_ms(MOTOR_PARK_BEAM_POLL_MS + 1u);
    motor_ctrl_run_until_idle(50u);

    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    TEST_ASSERT_TRUE(app_event_pending_type(EVT_PARK_DONE));
}


/* The auger rests parked with the index hole aligned (beam open). If the
 * initial beam read at burst start fails (expander busy), the first sample
 * must not be taken as a hole edge: a 1-portion burst would stop at once. */
void test_motor_ctrl_burst_from_parked_hole_needs_a_real_edge(void)
{
    uint32_t i;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(true);          /* parked in the hole */
    fake_motor_index_port_set_poll_busy_remaining(1u);  /* first read fails */
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(1u, 8000u));

    for (i = 0u; i < 20u; i++) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    TEST_ASSERT_TRUE(motor_ctrl_is_active());           /* still turning */

    fake_motor_index_port_set_beam_open(false);         /* leaves the hole */
    for (i = 0u; i < 32u; i++) {                        /* > MIN_CLOSED between holes */
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    TEST_ASSERT_TRUE(motor_ctrl_is_active());
    fake_motor_index_port_set_beam_open(true);          /* next hole: 1 portion */
    for (i = 0u; i < 5u && motor_ctrl_is_active(); i++) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    TEST_ASSERT_FALSE(motor_ctrl_is_active());
}

/* Parked at the hole edge, the start-up jolt flickers the beam closed→open
 * for a few ms. That is not a portion: the burst must keep turning. */
void test_motor_ctrl_start_jolt_flicker_is_not_a_portion(void)
{
    uint32_t i;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(true);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(1u, 8000u));
    motor_ctrl_test_poll();

    fake_motor_index_port_set_beam_open(false);         /* jolt: 50 ms closed */
    fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
    motor_ctrl_test_poll();
    fake_motor_index_port_set_beam_open(true);          /* back in the same hole */
    for (i = 0u; i < 4u; i++) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    fake_motor_index_port_set_beam_open(false);         /* real closed stretch */
    for (i = 0u; i < 32u; i++) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    fake_motor_index_port_set_beam_open(true);          /* next hole */
    for (i = 0u; i < 5u && motor_ctrl_is_active(); i++) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
    TEST_ASSERT_FALSE(motor_ctrl_is_active());
}

/* Field-measured timings (mains): hole every ~6400 ms, open ~200 ms. */
static void step_ms(uint32_t ms)
{
    uint32_t t;

    for (t = 0u; t < ms; t += MOTOR_BURST_BEAM_POLL_MS + 1u) {
        fake_time_advance_ms(MOTOR_BURST_BEAM_POLL_MS + 1u);
        motor_ctrl_test_poll();
    }
}

/* Device log: "baseline closed", "beam open +168 ms", "pulse 1", burst done,
 * bowl unchanged. The loaded auger sprang back out of the hole it stopped in;
 * re-entering it is not a portion. */
void test_motor_ctrl_springback_reentry_is_not_a_portion(void)
{
    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(false);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();

    step_ms(160u);
    fake_motor_index_port_set_beam_open(true);   /* same hole again */
    step_ms(200u);
    fake_motor_index_port_set_beam_open(false);
    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    step_ms(6000u);
    fake_motor_index_port_set_beam_open(true);   /* next hole, half a turn later */
    step_ms(120u);
    TEST_ASSERT_FALSE(motor_ctrl_is_active());
}

/* Device log: pulse 1 then no hole for 19.5 s until the 20 s session limit.
 * A stall after the first pulse must trip the per-gap index timeout. */
void test_motor_ctrl_stall_after_first_pulse_trips_index_timeout(void)
{
    char log[16384];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(false);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(3u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    step_ms(6000u);
    fake_motor_index_port_set_beam_open(true);   /* pulse 1 */
    step_ms(200u);
    fake_motor_index_port_set_beam_open(false);  /* then stalls */

    capture_stdout_begin(&saved, &cap);
    step_ms(MOTOR_BURST_TIMEOUT_MS + 300u);
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_NOT_NULL(strstr(log, "jam: index timeout"));
    TEST_ASSERT_NULL(strstr(log, "session timeout"));
}

/* 50 g = 5 portions at ~6.4 s per hole is ~32 s of normal rotation: it must
 * not hit the 20 s session limit. */
void test_motor_ctrl_five_portions_at_field_speed_complete(void)
{
    unsigned i;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(false);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(5u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    for (i = 0u; i < 5u; i++) {
        step_ms(6200u);
        fake_motor_index_port_set_beam_open(true);
        step_ms(200u);
        if (i < 4u) {
            TEST_ASSERT_TRUE(motor_ctrl_is_active());
        }
        fake_motor_index_port_set_beam_open(false);
    }
    TEST_ASSERT_FALSE(motor_ctrl_is_active());
}

/* Field trace: 280 mA for 8 s with the beam never changing, then the anti-jam
 * reverse freed it. Back off after the 2 s stall window instead. */
void test_motor_ctrl_stall_backs_off_after_two_seconds(void)
{
    char log[16384];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(false);
    fake_adc_port_set_motor_ma(280u);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));

    capture_stdout_begin(&saved, &cap);
    step_ms(MOTOR_STALL_MS + 300u);
    capture_stdout_end(saved, cap, log, sizeof(log));

    TEST_ASSERT_NOT_NULL(strstr(log, "jam: stall 280 mA"));
    TEST_ASSERT_NOT_NULL(strstr(log, "antijam: retry 1/3"));
    TEST_ASSERT_NULL(strstr(log, "index timeout"));
    fake_adc_port_set_motor_ma(0u);
}

/* Normal rotation current never trips the stall rule. */
void test_motor_ctrl_normal_load_is_not_a_stall(void)
{
    char log[16384];
    FILE *saved;
    FILE *cap;

    motor_ctrl_test_setup();
    fake_motor_index_port_set_beam_open(false);
    fake_adc_port_set_motor_ma(150u);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(1u, MOTOR_BURST_TIMEOUT_MS));
    capture_stdout_begin(&saved, &cap);
    step_ms(5000u);
    capture_stdout_end(saved, cap, log, sizeof(log));
    TEST_ASSERT_NULL(strstr(log, "jam:"));
    TEST_ASSERT_TRUE(motor_ctrl_is_active());
    fake_adc_port_set_motor_ma(0u);
}

/* A 2-portion burst that jams after the first hole must stop at the second
 * hole after recovery, not run two more. */
void test_motor_ctrl_antijam_resume_keeps_counted_pulses(void)
{
    motor_ctrl_test_setup();
    fake_adc_port_set_motor_ma(100u);
    fake_motor_index_port_set_beam_open(false);
    TEST_ASSERT_EQUAL(PORT_OK, motor_ctrl_request_burst(2u, MOTOR_BURST_TIMEOUT_MS));
    motor_ctrl_test_poll();
    step_ms(6000u);
    fake_motor_index_port_set_beam_open(true);    /* pulse 1 */
    step_ms(200u);
    fake_motor_index_port_set_beam_open(false);

    fake_adc_port_set_motor_ma(280u);             /* stalls */
    step_ms(MOTOR_STALL_MS + 200u);
    fake_adc_port_set_motor_ma(0u);               /* reverse, load drops */
    step_ms(MOTOR_ANTI_JAM_REVERSE_MS + 300u);
    fake_adc_port_set_motor_ma(100u);             /* resumed, turning */
    TEST_ASSERT_TRUE(motor_ctrl_is_active());

    step_ms(5000u);
    fake_motor_index_port_set_beam_open(true);    /* next hole = pulse 2 */
    step_ms(200u);
    TEST_ASSERT_FALSE(motor_ctrl_is_active());
    fake_adc_port_set_motor_ma(0u);
}
