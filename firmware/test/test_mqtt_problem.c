/* Tests: spec/30-processes/health-monitoring.md § Problem state (MQTT) */

#include <string.h>

#include "unity.h"

#include "fake_config_port.h"
#include "feed_history.h"
#include "mqtt_outbox.h"
#include "mqtt_problem.h"
#include "mqtt_route.h"
#include "problem.h"

void test_mqtt_problem_payload_and_clear_route(void)
{
    char buf[1024];

    fake_config_port_reset();
    feed_history_test_reset();
    problem_test_reset();
    TEST_ASSERT_GREATER_THAN(0, mqtt_problem_format(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"active\":false,\"codes\":[],\"text\":\"\"}", buf);

    problem_raise(PROBLEM_FEED_FAILED, "stuck");
    TEST_ASSERT_GREATER_THAN(0, mqtt_problem_format(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"active\":true,\"codes\":[\"E1\"],\"text\":\"Feed failed (stuck)\"}", buf);

    TEST_ASSERT_EQUAL(MQTT_ROUTE_CMD_PROBLEM_CLEAR,
                      mqtt_route_classify("petfeeder/ddeeff/cmd/problem_clear", "ddeeff"));
    TEST_ASSERT_EQUAL(MQTT_ROUTE_CMD_FEED_LIMIT,
                      mqtt_route_classify("petfeeder/ddeeff/cmd/feed_limit", "ddeeff"));
    TEST_ASSERT_EQUAL(PORT_OK, mqtt_problem_handle_clear());
    TEST_ASSERT_FALSE(problem_active());
}

void test_mqtt_problem_includes_last_feed(void)
{
    char buf[1024];
    feed_record_t r;

    fake_config_port_reset();
    feed_history_test_reset();
    problem_test_reset();
    memset(&r, 0, sizeof(r));
    r.src = 's';
    strcpy(r.res, "stuck");
    strcpy(r.why, "antijam");
    strcpy(r.jam, "stall");
    feed_history_add(&r);
    TEST_ASSERT_GREATER_THAN(0, mqtt_problem_format(buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",\"last_feed\":{\"t\":0,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"why\":\"antijam\""));
}
