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

void test_portion_estimate_converges_on_measured_portions(void)
{
    int i;

    setup();
    for (i = 0; i < 12; i++) {
        portion_estimate_learn(255, 3u);   /* field: 25.5 g for 3 holes */
    }
    TEST_ASSERT_INT32_WITHIN(3, 85, portion_estimate_dg());
    /* 30 g now needs 4 portions (34 g) instead of 3 (25.5 g) */
    TEST_ASSERT_EQUAL_UINT8(4u, portion_estimate_portions_for_dg(300, 20u));
    TEST_ASSERT_EQUAL_UINT8(1u, portion_estimate_portions_for_dg(100, 20u));
}

void test_portion_estimate_persists_and_ignores_outliers(void)
{
    setup();
    portion_estimate_learn(85, 1u);
    portion_estimate_learn(5, 1u);      /* nothing fell: ignored */
    portion_estimate_learn(900, 1u);    /* bowl knocked: ignored */
    portion_estimate_test_reset();      /* reboot: reload from config */
    TEST_ASSERT_EQUAL_INT32(96, portion_estimate_dg());   /* 100 + round(-15/4) */
}
