/* Tests: spec/30-processes/health-monitoring.md § Daily limit, § Not-eaten alert */

#include <string.h>

#include "unity.h"

#include "eaten_today.h"
#include "fake_config_port.h"
#include "feed_history.h"
#include "feed_limit.h"
#include "problem.h"

static void setup(void)
{
    fake_config_port_reset();
    feed_history_test_reset();
    feed_limit_test_reset();
    problem_test_reset();
}

void test_feed_limit_off_by_default(void)
{
    uint32_t rem = 0u;

    setup();
    TEST_ASSERT_EQUAL_UINT16(0u, feed_limit_daily_max_g());
    TEST_ASSERT_FALSE(feed_limit_remaining_g(&rem));
}

void test_feed_limit_cmd_sets_and_validates(void)
{
    char buf[96];

    setup();
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_feed_limit_handle("{\"daily_max_g\":200}", 19u));
    TEST_ASSERT_EQUAL_UINT16(200u, feed_limit_daily_max_g());
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_feed_limit_handle("{\"not_eaten_h\":6}", 17u));
    TEST_ASSERT_EQUAL_UINT8(6u, feed_limit_not_eaten_h());
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_feed_limit_handle("{\"daily_max_g\":5}", 17u));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, mqtt_feed_limit_handle("{\"not_eaten_h\":30}", 18u));
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_feed_limit_handle("{\"daily_max_g\":0}", 17u));
    TEST_ASSERT_EQUAL_UINT16(0u, feed_limit_daily_max_g());
    feed_limit_test_reset();                     /* reboot: stored */
    TEST_ASSERT_EQUAL_UINT8(6u, feed_limit_not_eaten_h());
    TEST_ASSERT_GREATER_THAN(0, mqtt_feed_limit_format(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"daily_max_g\":0,\"today_g\":0,\"not_eaten_h\":6}", buf);
}

void test_feed_not_eaten_raises_and_clears(void)
{
    setup();
    TEST_ASSERT_TRUE(feed_limit_set_not_eaten_h(2u));
    feed_not_eaten_poll(100, 0u);
    feed_not_eaten_on_feed(300, 1000u);
    feed_not_eaten_poll(120, 3600000u);               /* 2 g eaten, 1 h */
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_NOT_EATEN));
    feed_not_eaten_poll(130, 1000u + 7200000u);       /* 2 h, 3 g total */
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_NOT_EATEN));
    feed_not_eaten_poll(0, 1000u + 7300000u);         /* midnight reset */
    feed_not_eaten_poll(30, 1000u + 7400000u);        /* 3 g since the alert */
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_NOT_EATEN));
    feed_not_eaten_poll(60, 1000u + 7500000u);        /* 6 g since the alert */
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_NOT_EATEN));
}

void test_feed_not_eaten_new_feed_restarts_timer(void)
{
    setup();
    TEST_ASSERT_TRUE(feed_limit_set_not_eaten_h(1u));
    feed_not_eaten_poll(0, 0u);
    feed_not_eaten_on_feed(300, 0u);
    feed_not_eaten_poll(0, 3000000u);
    feed_not_eaten_on_feed(300, 3000000u);            /* new meal at 50 min */
    feed_not_eaten_poll(0, 3700000u);                 /* 1 h after the first only */
    TEST_ASSERT_FALSE(problem_is_set(PROBLEM_NOT_EATEN));
    feed_not_eaten_poll(0, 6700000u);
    TEST_ASSERT_TRUE(problem_is_set(PROBLEM_NOT_EATEN));
}
