/* Tests: spec/30-processes/ota-flow.md § Pre-download memory reclaim (app polls) */

#include <string.h>

#include "unity.h"

#include "app.h"
#include "app_event.h"
#include "app_event_port.h"
#include "fake_display_port.h"
#include "fake_weight_port.h"
#include "feeder_runtime.h"

extern void fake_app_event_q_reset(void);

static void post_event(app_event_type_t type)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    TEST_ASSERT_TRUE(app_event_post(&ev));
    app_step();
}

static void post_display_tick(uint32_t now_ms)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = EVT_DISPLAY_TICK;
    ev.u.display_tick.now_ms = now_ms;
    TEST_ASSERT_TRUE(app_event_post(&ev));
    app_step();
}

static size_t weight_ops(void)
{
    size_t n = 0;

    (void)fake_weight_port_ops(&n);
    return n;
}

static void boot_idle(void)
{
    fake_weight_port_reset();
    fake_display_port_reset();
    fake_app_event_q_reset();
    app_test_reset();
    app_event_port_init();
    feeder_runtime_test_reset();
    fake_weight_port_set_cal_status(WEIGHT_CAL_SUCCESS);
    fake_weight_port_set_read_dg(120);

    post_event(EVT_APP_BOOT);
    post_event(EVT_TIMER_TICK);
    post_event(EVT_TIMER_TICK);
}

void test_app_idle_weight_sampling_pauses_while_ota_active(void)
{
    size_t before;

    boot_idle();
    post_display_tick(1000u);
    post_display_tick(1500u);
    TEST_ASSERT_GREATER_THAN(0, weight_ops());

    feeder_runtime_set_ota_active(true);
    before = weight_ops();
    post_display_tick(2000u);
    post_display_tick(2500u);
    post_display_tick(3000u);
    TEST_ASSERT_EQUAL_UINT(before, weight_ops());

    feeder_runtime_set_ota_active(false);
    post_display_tick(3500u);
    TEST_ASSERT_GREATER_THAN(before, weight_ops());
}
