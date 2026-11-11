/* Tests: spec/30-processes/time-sync.md § Server */

#include "unity.h"

#include "fake_config_port.h"
#include "ntp_config.h"

void test_ntp_config_default_and_set(void)
{
    char buf[64];

    fake_config_port_reset();
    ntp_config_get(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", buf);
    TEST_ASSERT_EQUAL(PORT_OK, ntp_config_set("10.20.10.1"));
    ntp_config_get(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("10.20.10.1", buf);
    TEST_ASSERT_EQUAL(PORT_OK, ntp_config_set(""));
    ntp_config_get(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", buf);
}

void test_ntp_config_rejects_bad_names(void)
{
    fake_config_port_reset();
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, ntp_config_set("bad host"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, ntp_config_set("http://x"));
    TEST_ASSERT_EQUAL(PORT_ERR_INVALID_ARG, ntp_config_set("-x"));
}
