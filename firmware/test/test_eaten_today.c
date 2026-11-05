/* Tests: spec/30-processes/eaten-today.md */

#include "unity.h"

#include "config_keys.h"
#include "eaten_today.h"
#include "fake_config_port.h"

#define DAY1 20261105u
#define DAY2 20261106u

static void setup(void)
{
    fake_config_port_reset();
    eaten_today_test_reset();
    eaten_today_init(DAY1);
}

/* Feed n identical samples. */
static void feed(weight_dg_t dg, unsigned n, uint32_t day)
{
    for (unsigned i = 0u; i < n; i++) {
        eaten_today_on_idle_sample(dg, true, day);
    }
}

void test_eaten_today_starts_at_zero(void)
{
    setup();
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_counts_settled_drop(void)
{
    setup();
    feed(500, 8u, DAY1);   /* anchor + settle at 50.0 g */
    feed(420, 8u, DAY1);   /* pet ate 8.0 g */
    TEST_ASSERT_EQUAL_INT32(80, eaten_today_total_dg());
}

void test_eaten_today_ignores_drop_until_settled(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(420, 2u, DAY1);   /* still moving, not settled */
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_ignores_sub_gram_drop(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(493, 8u, DAY1);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_accumulates_across_meals(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(450, 8u, DAY1);   /* ate 5 g */
    feed(300, 8u, DAY1);   /* ate 15 g */
    TEST_ASSERT_EQUAL_INT32(200, eaten_today_total_dg());
}

void test_eaten_today_rise_is_never_counted_and_rebases(void)
{
    setup();
    feed(300, 8u, DAY1);
    feed(600, 8u, DAY1);   /* food added */
    feed(550, 8u, DAY1);   /* ate 5 g from the new level */
    TEST_ASSERT_EQUAL_INT32(50, eaten_today_total_dg());
}

void test_eaten_today_invalidate_prevents_counting_dispense_jump(void)
{
    setup();
    feed(500, 8u, DAY1);
    eaten_today_invalidate();
    feed(900, 8u, DAY1);   /* dispense added 40 g: rebased, not eaten */
    feed(900, 8u, DAY1);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
    feed(850, 8u, DAY1);
    TEST_ASSERT_EQUAL_INT32(50, eaten_today_total_dg());
}

void test_eaten_today_unusable_samples_do_not_count_and_rebase(void)
{
    setup();
    feed(500, 8u, DAY1);
    eaten_today_on_idle_sample(100, false, DAY1);   /* bowl removed */
    feed(900, 8u, DAY1);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_midnight_resets_total(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(400, 8u, DAY1);
    TEST_ASSERT_EQUAL_INT32(100, eaten_today_total_dg());
    feed(400, 1u, DAY2);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
    feed(300, 8u, DAY2);
    TEST_ASSERT_EQUAL_INT32(100, eaten_today_total_dg());
}

void test_eaten_today_unknown_clock_keeps_accumulating(void)
{
    setup();
    feed(500, 8u, 0u);
    feed(400, 8u, 0u);
    feed(300, 8u, 0u);
    TEST_ASSERT_EQUAL_INT32(200, eaten_today_total_dg());
}

void test_eaten_today_persists_and_restores_same_day(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(400, 8u, DAY1);
    eaten_today_test_reset();
    eaten_today_init(DAY1);
    TEST_ASSERT_EQUAL_INT32(100, eaten_today_total_dg());
}

void test_eaten_today_restore_drops_stale_day(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(400, 8u, DAY1);
    eaten_today_test_reset();
    eaten_today_init(DAY2);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_restore_with_unknown_clock_keeps_total(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(400, 8u, DAY1);
    eaten_today_test_reset();
    eaten_today_init(0u);
    TEST_ASSERT_EQUAL_INT32(100, eaten_today_total_dg());
}

void test_eaten_today_clock_arrives_after_restore_same_day_keeps_total(void)
{
    setup();
    feed(500, 8u, DAY1);
    feed(400, 8u, DAY1);
    eaten_today_test_reset();
    eaten_today_init(0u);
    feed(400, 2u, DAY1);
    TEST_ASSERT_EQUAL_INT32(100, eaten_today_total_dg());
    feed(400, 2u, DAY2);
    TEST_ASSERT_EQUAL_INT32(0, eaten_today_total_dg());
}

void test_eaten_today_old_schema_total_is_zeroed_once(void)
{
    const config_port_t *cfg = fake_config_port_get();

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(PORT_OK, cfg->write(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_DAY, "20261105"));
    TEST_ASSERT_EQUAL(PORT_OK, cfg->write(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_DG, "3500"));
    eaten_today_init(20261105u);
    TEST_ASSERT_EQUAL(0, eaten_today_total_dg());
}

void test_eaten_today_reset_zeroes_and_persists(void)
{
    const config_port_t *cfg = fake_config_port_get();
    char buf[16];

    fake_config_port_reset();
    eaten_today_init(20261105u);
    eaten_today_on_idle_sample(500, true, 20261105u);
    eaten_today_reset();
    TEST_ASSERT_EQUAL(0, eaten_today_total_dg());
    TEST_ASSERT_EQUAL(PORT_OK, cfg->read(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_DG, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("0", buf);
}
