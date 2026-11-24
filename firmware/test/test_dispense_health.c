/* Tests: spec/30-processes/health-monitoring.md (history, problems, daily limit,
 * not-eaten) and dispense-cycle.md § Portion size (learning) — dispense wiring */

#include <string.h>

#include "unity.h"

#include "app.h"
#include "app_event.h"
#include "app_event_port.h"
#include "auto_tare.h"
#include "dispense.h"
#include "dispense_cli.h"
#include "display_presentation.h"
#include "fake_config_port.h"
#include "fake_display_port.h"
#include "fake_motor_port.h"
#include "fake_time.h"
#include "fake_weight_port.h"
#include "feed_bowl.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "feeder_runtime.h"
#include "motor_port_provider_host.h"
#include "portion_estimate.h"
#include "problem.h"
#include "schedule.h"
#include "weight_units.h"

extern void fake_app_event_q_reset(void);

static void setup(void)
{
    fake_time_reset();
    fake_config_port_reset();
    motor_port_host_reset();
    fake_motor_port_reset();
    fake_weight_port_reset();
    fake_weight_port_set_cal_status(WEIGHT_CAL_SUCCESS);
    fake_app_event_q_reset();
    fake_display_port_reset();
    display_presentation_reset();
    dispense_test_reset();
    dispense_cli_test_reset();
    app_test_reset();
    app_event_port_init();
    auto_tare_test_reset();
    feeder_runtime_test_reset();
    feed_history_test_reset();
    feed_limit_test_reset();
    problem_test_reset();
}

static void seed_bowl(int32_t grams)
{
    weight_dg_t dg = WEIGHT_G_TO_DG(grams);

    fake_weight_port_set_read_dg(dg);
    auto_tare_anchor(dg);
    app_bowl_dg_notify_read(dg, true, 0u);
}

/* Run one job: motor event, then the settle with the bowl at `after_g`. */
static void finish_job(app_event_type_t motor_event, int32_t after_g)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = motor_event;
    TEST_ASSERT_TRUE(app_event_post(&ev));
    TEST_ASSERT_TRUE(app_step());
    fake_weight_port_set_read_dg(WEIGHT_G_TO_DG(after_g));
    dispense_poll(1000u);
    dispense_poll(1000u + DISPENSE_SETTLE_MS);
    TEST_ASSERT_FALSE(dispense_is_active());
    fake_motor_port_set_active(false);
}

void test_dispense_records_history_for_each_job(void)
{
    feed_record_t r;

    setup();
    seed_bowl(0);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_SCHEDULE));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 30);

    TEST_ASSERT_EQUAL_UINT(1u, feed_history_count());
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_CHAR('s', r.src);
    TEST_ASSERT_EQUAL_UINT16(30u, r.want);
    TEST_ASSERT_EQUAL_INT16(30, r.got);
    TEST_ASSERT_TRUE(r.meas);
    TEST_ASSERT_EQUAL_STRING("ok", r.res);
    TEST_ASSERT_EQUAL_UINT8(3u, r.por);
    TEST_ASSERT_TRUE(r.bowl);
    TEST_ASSERT_EQUAL_UINT32(1u, feed_counter_get(FEED_COUNTER_OK));
    TEST_ASSERT_FALSE(problem_active());
}

/* "failed" with no explanation: a stuck job must leave a record and raise E1;
 * the next good feed clears it. */
void test_dispense_stuck_raises_feed_failed_until_next_ok(void)
{
    feed_record_t r;

    setup();
    seed_bowl(0);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_SCHEDULE));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_MOTOR_FAULT, 0);

    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_STRING("stuck", r.res);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_FEED_FAILED));
    TEST_ASSERT_EQUAL_UINT32(1u, feed_counter_get(FEED_COUNTER_FAILED));

    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(10u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 10);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_FEED_FAILED));
}

/* Motor counted holes but the bowl did not move: E1 "nothing fell". */
void test_dispense_nothing_fell_raises_feed_failed(void)
{
    setup();
    seed_bowl(20);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(20u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 20);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_FEED_FAILED));
}

/* The job still runs without the bowl, with an alert. */
void test_dispense_bowl_missing_still_dispenses_with_alert(void)
{
    feed_record_t r;

    setup();
    seed_bowl(-300);                                  /* bowl lifted */
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(20u, DISPENSE_SOURCE_SCHEDULE));
    TEST_ASSERT_TRUE(app_step());
    TEST_ASSERT_EQUAL(1u, fake_motor_port_burst_calls());
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_BOWL_MISSING));
    finish_job(EVT_BURST_DONE, -300);
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_FALSE(r.bowl);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_FEED_FAILED));
}

void test_dispense_daily_limit_shortens_then_refuses(void)
{
    feed_record_t r;

    setup();
    TEST_ASSERT_TRUE(feed_limit_set_daily_max_g(40u));
    seed_bowl(0);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_SCHEDULE));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 30);
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_DAILY_LIMIT));

    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_TRUE(app_step());
    TEST_ASSERT_EQUAL(1u, fake_motor_port_last_pulse_target());   /* 10 g left */
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_DAILY_LIMIT));
    finish_job(EVT_BURST_DONE, 40);

    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_TRUE(app_step());
    TEST_ASSERT_EQUAL(2u, fake_motor_port_burst_calls());          /* refused */
    TEST_ASSERT_FALSE(dispense_is_active());
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_STRING("limit", r.res);
}

void test_schedule_fire_skips_when_daily_limit_reached(void)
{
    feed_record_t r;

    setup();
    TEST_ASSERT_TRUE(feed_limit_set_daily_max_g(20u));
    seed_bowl(0);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(20u, DISPENSE_SOURCE_MQTT));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 20);

    TEST_ASSERT_EQUAL(SCHEDULE_FIRE_SKIPPED_LIMIT, feed_schedule_fire(30u, 5000u));
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_STRING("limit", r.res);
    TEST_ASSERT_EQUAL_CHAR('s', r.src);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_DAILY_LIMIT));
}

void test_dispense_weighed_feed_refines_portion(void)
{
    setup();
    seed_bowl(0);
    TEST_ASSERT_EQUAL(DISPENSE_SUBMIT_OK, dispense_submit_grams(30u, DISPENSE_SOURCE_SCHEDULE));
    TEST_ASSERT_TRUE(app_step());
    finish_job(EVT_BURST_DONE, 36);                   /* 12 g per hole */
    TEST_ASSERT_TRUE(portion_estimate_dg() > 100);
    TEST_ASSERT_EQUAL(PORTION_CAL_LEARNED, portion_estimate_status());
}
