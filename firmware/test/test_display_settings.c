/* Tests: spec/30-processes/config-store.md (display group),
 *        spec/30-processes/mqtt-protocol.md § Display */

#include "unity.h"

#include "config_keys.h"
#include "fake_config_port.h"
#include "display_settings.h"

static void setup(void)
{
    fake_config_port_reset();
}

void test_display_settings_defaults_are_weight_and_level_4(void)
{
    setup();
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
    TEST_ASSERT_EQUAL_UINT8(4u, display_settings_brightness_get());
}

void test_display_settings_mode_round_trips_through_nvdm(void)
{
    setup();
    TEST_ASSERT_TRUE(display_settings_mode_set(DISPLAY_MODE_OFF));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_OFF, display_settings_mode_get());
    TEST_ASSERT_TRUE(display_settings_mode_set(DISPLAY_MODE_WEIGHT));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
}

void test_display_settings_brightness_round_trips_and_rejects_range(void)
{
    setup();
    TEST_ASSERT_TRUE(display_settings_brightness_set(1u));
    TEST_ASSERT_EQUAL_UINT8(1u, display_settings_brightness_get());
    TEST_ASSERT_FALSE(display_settings_brightness_set(0u));
    TEST_ASSERT_FALSE(display_settings_brightness_set(5u));
    TEST_ASSERT_EQUAL_UINT8(1u, display_settings_brightness_get());
}

void test_display_settings_corrupt_values_fall_back_to_defaults(void)
{
    char buf[8];

    setup();
    TEST_ASSERT_EQUAL(PORT_OK,
                      config_port_get()->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_MODE, "bogus"));
    TEST_ASSERT_EQUAL(PORT_OK,
                      config_port_get()->write(CONFIG_GROUP_DISPLAY, CONFIG_KEY_DISPLAY_BRIGHTNESS, "9"));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_WEIGHT, display_settings_mode_get());
    TEST_ASSERT_EQUAL_UINT8(4u, display_settings_brightness_get());
    (void)buf;
}

void test_display_settings_mode_names_parse_and_format(void)
{
    display_mode_t m;

    TEST_ASSERT_EQUAL_STRING("weight", display_settings_mode_name(DISPLAY_MODE_WEIGHT));
    TEST_ASSERT_EQUAL_STRING("off", display_settings_mode_name(DISPLAY_MODE_OFF));
    TEST_ASSERT_TRUE(display_settings_mode_parse("off", &m));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_OFF, m);
    TEST_ASSERT_TRUE(display_settings_mode_parse("eaten_today", &m));
    TEST_ASSERT_EQUAL(DISPLAY_MODE_EATEN_TODAY, m);
    TEST_ASSERT_EQUAL_STRING("eaten_today", display_settings_mode_name(DISPLAY_MODE_EATEN_TODAY));
    TEST_ASSERT_FALSE(display_settings_mode_parse("sparkle", &m));
    TEST_ASSERT_FALSE(display_settings_mode_parse("", &m));
    TEST_ASSERT_FALSE(display_settings_mode_parse(NULL, &m));
}
