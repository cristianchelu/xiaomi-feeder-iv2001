/* Tests: spec/30-processes/health-monitoring.md § Task watch */

#include "unity.h"

#include "fake_config_port.h"
#include "feed_history.h"
#include "problem.h"
#include "task_watch.h"

void test_task_watch_reports_stall_once_and_resume(void)
{
    fake_config_port_reset();
    feed_history_test_reset();
    problem_test_reset();
    task_watch_test_reset();

    task_watch_beat(TASK_WATCH_MOTOR);
    TEST_ASSERT_FALSE(task_watch_check(TASK_WATCH_MOTOR, 1000u));
    task_watch_beat(TASK_WATCH_MOTOR);
    TEST_ASSERT_FALSE(task_watch_check(TASK_WATCH_MOTOR, 30000u));
    TEST_ASSERT_FALSE(task_watch_check(TASK_WATCH_MOTOR, 30000u + TASK_WATCH_STALL_MS - 1u));
    TEST_ASSERT_TRUE(task_watch_check(TASK_WATCH_MOTOR, 30000u + TASK_WATCH_STALL_MS));
    TEST_ASSERT_TRUE(task_watch_check(TASK_WATCH_MOTOR, 200000u));
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_TASK_STALL));
    TEST_ASSERT_EQUAL_UINT32(1u, feed_counter_get(FEED_COUNTER_STALLS));

    task_watch_beat(TASK_WATCH_MOTOR);
    TEST_ASSERT_FALSE(task_watch_check(TASK_WATCH_MOTOR, 200500u));
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_TASK_STALL));      /* latched */
}
