/* Tests: spec/30-processes/health-monitoring.md § Feed history, § Health counters */

#include <string.h>

#include "unity.h"

#include "fake_config_port.h"
#include "feed_history.h"

static feed_record_t rec(uint32_t t, uint32_t day, int16_t got, const char *res)
{
    feed_record_t r;

    memset(&r, 0, sizeof(r));
    r.t = t;
    r.day = day;
    r.src = 's';
    r.want = 30u;
    r.got = got;
    r.meas = true;
    strcpy(r.res, res);
    r.por = 3u;
    r.pul = 3u;
    strcpy(r.why, "-");
    strcpy(r.jam, "-");
    r.ok = 120u;
    r.ma = 150u;
    r.dur = 19u;
    r.mqtt = true;
    r.bowl = true;
    return r;
}

static void setup(void)
{
    fake_config_port_reset();
    feed_history_test_reset();
}

void test_feed_history_newest_first_and_ring(void)
{
    feed_record_t r;
    unsigned i;

    setup();
    for (i = 0u; i < FEED_HISTORY_DEPTH + 3u; i++) {
        r = rec(1000u + i, 20261111u, (int16_t)i, "ok");
        feed_history_add(&r);
    }
    TEST_ASSERT_EQUAL_UINT(FEED_HISTORY_DEPTH, feed_history_count());
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_UINT32(1000u + FEED_HISTORY_DEPTH + 2u, r.t);
    TEST_ASSERT_TRUE(feed_history_get(FEED_HISTORY_DEPTH - 1u, &r));
    TEST_ASSERT_EQUAL_UINT32(1003u, r.t);
    TEST_ASSERT_FALSE(feed_history_get(FEED_HISTORY_DEPTH, &r));
}

void test_feed_history_survives_reboot(void)
{
    feed_record_t r = rec(1794219015u, 20261111u, 28, "stuck");

    setup();
    strcpy(r.why, "antijam");
    strcpy(r.jam, "stall");
    r.aj = 3u;
    r.busy = 7u;
    feed_history_add(&r);
    feed_history_test_reset();     /* RAM lost, NVDM kept */
    TEST_ASSERT_EQUAL_UINT(1u, feed_history_count());
    TEST_ASSERT_TRUE(feed_history_get(0u, &r));
    TEST_ASSERT_EQUAL_STRING("stuck", r.res);
    TEST_ASSERT_EQUAL_STRING("antijam", r.why);
    TEST_ASSERT_EQUAL_STRING("stall", r.jam);
    TEST_ASSERT_EQUAL_UINT(3u, r.aj);
    TEST_ASSERT_EQUAL_UINT(7u, r.busy);
    TEST_ASSERT_EQUAL_INT(28, r.got);
}

void test_feed_history_json(void)
{
    char buf[3000];
    feed_record_t r = rec(1794219015u, 20261111u, 28, "ok");

    setup();
    TEST_ASSERT_GREATER_THAN(0, feed_history_format_json(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("[]", buf);
    feed_history_add(&r);
    TEST_ASSERT_GREATER_THAN(0, feed_history_format_json(buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"t\":1794219015"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"src\":\"s\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"res\":\"ok\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"busy\":0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bowl\":1"));
}

void test_feed_history_grams_on_day(void)
{
    feed_record_t r;

    setup();
    r = rec(1u, 20261110u, 50, "ok");
    feed_history_add(&r);
    r = rec(2u, 20261111u, 30, "ok");
    feed_history_add(&r);
    r = rec(3u, 20261111u, 0, "stuck");
    feed_history_add(&r);
    r = rec(4u, 20261111u, 12, "underfill");
    feed_history_add(&r);
    TEST_ASSERT_EQUAL_UINT32(42u, feed_history_grams_on_day(20261111u));
    TEST_ASSERT_EQUAL_UINT32(50u, feed_history_grams_on_day(20261110u));
    TEST_ASSERT_EQUAL_UINT32(0u, feed_history_grams_on_day(0u));
}

void test_feed_counters_persist(void)
{
    setup();
    feed_counter_inc(FEED_COUNTER_OK);
    feed_counter_inc(FEED_COUNTER_OK);
    feed_counter_inc(FEED_COUNTER_MQTT_LOST);
    feed_history_test_reset();
    TEST_ASSERT_EQUAL_UINT32(2u, feed_counter_get(FEED_COUNTER_OK));
    TEST_ASSERT_EQUAL_UINT32(1u, feed_counter_get(FEED_COUNTER_MQTT_LOST));
    TEST_ASSERT_EQUAL_UINT32(0u, feed_counter_get(FEED_COUNTER_JAMS));
}
