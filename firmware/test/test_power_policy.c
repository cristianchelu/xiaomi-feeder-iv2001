/* Tests: spec/30-processes/power-state-machine.md */

#include "unity.h"

#include "power_policy.h"

static power_policy_input_t base(void)
{
    power_policy_input_t in = {
        .on_battery = true, .asleep = false, .low_battery = false,
        .mode = BATTERY_WIFI_ON, .next_in_min = -1, .dispense_recent = false,
    };
    return in;
}

void test_power_policy_mains_always_wants_wifi(void)
{
    power_policy_input_t in = base();

    in.on_battery = false;
    in.mode = BATTERY_WIFI_OFF;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
}

void test_power_policy_battery_modes(void)
{
    power_policy_input_t in = base();

    in.mode = BATTERY_WIFI_ON;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
    in.mode = BATTERY_WIFI_OFF;
    TEST_ASSERT_FALSE(power_policy_wifi_wanted(&in));
    in.mode = BATTERY_WIFI_SCHEDULED_ONLY;
    TEST_ASSERT_FALSE(power_policy_wifi_wanted(&in));
}

void test_power_policy_scheduled_only_window(void)
{
    power_policy_input_t in = base();

    in.mode = BATTERY_WIFI_SCHEDULED_ONLY;
    in.next_in_min = 3;
    TEST_ASSERT_FALSE(power_policy_wifi_wanted(&in));
    in.next_in_min = 2;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
    in.next_in_min = 0;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
    in.next_in_min = 50;
    in.dispense_recent = true;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
}

void test_power_policy_sleep_turns_wifi_off_everywhere(void)
{
    power_policy_input_t in = base();

    in.on_battery = false;
    in.asleep = true;
    TEST_ASSERT_FALSE(power_policy_wifi_wanted(&in));
}

void test_power_policy_low_battery_downgrades_on_to_scheduled_only(void)
{
    power_policy_input_t in = base();

    in.low_battery = true;
    TEST_ASSERT_FALSE(power_policy_wifi_wanted(&in));
    in.next_in_min = 1;
    TEST_ASSERT_TRUE(power_policy_wifi_wanted(&in));
}

void test_power_policy_screen_off_delay(void)
{
    TEST_ASSERT_EQUAL_UINT16(60u, power_policy_screen_off_s(60u, true, false));
    TEST_ASSERT_EQUAL_UINT16(10u, power_policy_screen_off_s(60u, true, true));
    TEST_ASSERT_EQUAL_UINT16(10u, power_policy_screen_off_s(0u, true, true));
    TEST_ASSERT_EQUAL_UINT16(5u, power_policy_screen_off_s(5u, true, true));
    TEST_ASSERT_EQUAL_UINT16(0u, power_policy_screen_off_s(0u, false, true));
}

void test_power_policy_names_round_trip(void)
{
    battery_wifi_t m;

    TEST_ASSERT_TRUE(power_policy_battery_wifi_parse("scheduled_only", &m));
    TEST_ASSERT_EQUAL(BATTERY_WIFI_SCHEDULED_ONLY, m);
    TEST_ASSERT_EQUAL_STRING("off", power_policy_battery_wifi_name(BATTERY_WIFI_OFF));
    TEST_ASSERT_FALSE(power_policy_battery_wifi_parse("sometimes", &m));
}
