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
