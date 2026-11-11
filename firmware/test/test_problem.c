/* Tests: spec/30-processes/health-monitoring.md § Problem state */

#include <string.h>

#include "unity.h"

#include "problem.h"

void test_problem_latched_and_live(void)
{
    char buf[200];

    problem_test_reset();
    TEST_ASSERT_FALSE(problem_active());
    TEST_ASSERT_EQUAL(PROBLEM_NONE, problem_top());

    problem_raise(PROBLEM_BOWL_MISSING, NULL);
    problem_set_live(PROBLEM_TIME, true);
    problem_raise(PROBLEM_FEED_FAILED, "stuck");
    TEST_ASSERT_TRUE(problem_active());
    TEST_ASSERT_EQUAL(PROBLEM_FEED_FAILED, problem_top());
    problem_format_codes(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("[\"E1\",\"E3\",\"E5\"]", buf);
    problem_format_text(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Feed failed (stuck); Bowl missing at feed; Time not synced", buf);

    problem_clear_latched();               /* button: latched only */
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_TIME));
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_FEED_FAILED));
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_BOWL_MISSING));
    problem_set_live(PROBLEM_TIME, false);
    TEST_ASSERT_FALSE(problem_active());
}

void test_problem_priority_and_generation(void)
{
    uint32_t g;

    problem_test_reset();
    g = problem_generation();
    problem_raise(PROBLEM_CONNECTION, "12 min");
    problem_raise(PROBLEM_NOT_EATEN, NULL);
    problem_set_live(PROBLEM_HOPPER_EMPTY, true);
    TEST_ASSERT_NOT_EQUAL(g, problem_generation());
    TEST_ASSERT_EQUAL(PROBLEM_HOPPER_EMPTY, problem_top());
    problem_set_live(PROBLEM_HOPPER_EMPTY, false);
    TEST_ASSERT_EQUAL(PROBLEM_NOT_EATEN, problem_top());
    g = problem_generation();
    problem_set_live(PROBLEM_HOPPER_EMPTY, false);      /* no change */
    TEST_ASSERT_EQUAL_UINT32(g, problem_generation());
    problem_clear(PROBLEM_NOT_EATEN);
    TEST_ASSERT_EQUAL(PROBLEM_CONNECTION, problem_top());
}
