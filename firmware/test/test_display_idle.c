/* Tests: spec/30-processes/display-presentation.md § Idle blank on battery */

#include "unity.h"

#include "display_idle.h"

void test_display_idle_never_blanks_on_mains(void)
{
    display_idle_reset(0u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(1000000u, false, 60u));
}

void test_display_idle_blanks_on_battery_after_timeout(void)
{
    display_idle_reset(1000u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(1000u + 59999u, true, 60u));
    TEST_ASSERT_TRUE(display_idle_blank_wanted(1000u + 60000u, true, 60u));
}

void test_display_idle_zero_timeout_disables(void)
{
    display_idle_reset(0u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(10000000u, true, 0u));
}

void test_display_idle_activity_restarts_countdown(void)
{
    display_idle_reset(0u);
    display_idle_note_activity(50000u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(60000u, true, 60u));
    TEST_ASSERT_TRUE(display_idle_blank_wanted(110000u, true, 60u));
    display_idle_note_activity(110000u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(110001u, true, 60u));
}

void test_display_idle_handles_tick_wraparound(void)
{
    display_idle_reset(0xFFFFFF00u);
    TEST_ASSERT_FALSE(display_idle_blank_wanted(0xFFFFFF00u + 1000u, true, 60u));
    TEST_ASSERT_TRUE(display_idle_blank_wanted(0xFFFFFF00u + 60000u, true, 60u));
}
