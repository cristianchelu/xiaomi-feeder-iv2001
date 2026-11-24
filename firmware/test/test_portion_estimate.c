/* Tests: spec/30-processes/dispense-cycle.md § Portion size */

#include "unity.h"

#include "fake_config_port.h"
#include "portion_estimate.h"

static void setup(void)
{
    fake_config_port_reset();
    portion_estimate_test_reset();
}

void test_portion_estimate_defaults_to_10_g(void)
{
    setup();
    TEST_ASSERT_EQUAL_INT32(100, portion_estimate_dg());
    TEST_ASSERT_EQUAL_UINT8(1u, portion_estimate_portions_for_dg(100, 20u));
    TEST_ASSERT_EQUAL_UINT8(3u, portion_estimate_portions_for_dg(300, 20u));
}

void test_portion_estimate_calibration_persists_across_reboot(void)
{
    setup();
    TEST_ASSERT_TRUE(portion_estimate_calibrate(255, 3u));
    portion_estimate_test_reset();      /* reboot: reload from config */
    TEST_ASSERT_EQUAL_INT32(85, portion_estimate_dg());
    TEST_ASSERT_EQUAL(PORTION_CAL_OK, portion_estimate_status());
    /* 30 g now needs 4 portions (34 g) instead of 3 (25.5 g) */
    TEST_ASSERT_EQUAL_UINT8(4u, portion_estimate_portions_for_dg(300, 20u));
    TEST_ASSERT_EQUAL_UINT8(1u, portion_estimate_portions_for_dg(100, 20u));
}

/* 10 g is only the starting average: weighed feeds pull the estimate. */
void test_portion_estimate_learns_from_feeds(void)
{
    unsigned i;

    fake_config_port_reset();
    portion_estimate_test_reset();
    for (i = 0u; i < 40u; i++) {
        TEST_ASSERT_TRUE(portion_estimate_learn(3 * 85, 3u));   /* 8.5 g per hole */
    }
    TEST_ASSERT_INT32_WITHIN(1, 85, portion_estimate_dg());
    TEST_ASSERT_EQUAL(PORTION_CAL_LEARNED, portion_estimate_status());
    portion_estimate_test_reset();                               /* reboot: stored */
    TEST_ASSERT_INT32_WITHIN(2, 85, portion_estimate_dg());
}

void test_portion_estimate_learning_ignores_outliers(void)
{
    fake_config_port_reset();
    portion_estimate_test_reset();
    TEST_ASSERT_FALSE(portion_estimate_learn(3 * 400, 3u));     /* 40 g per hole */
    TEST_ASSERT_FALSE(portion_estimate_learn(10, 3u));          /* pet ate */
    TEST_ASSERT_FALSE(portion_estimate_learn(100, 0u));
    TEST_ASSERT_EQUAL_INT32(100, portion_estimate_dg());
    TEST_ASSERT_TRUE(portion_estimate_learn(120, 1u));          /* one 12 g feed */
    TEST_ASSERT_INT32_WITHIN(1, 102, portion_estimate_dg());
}
