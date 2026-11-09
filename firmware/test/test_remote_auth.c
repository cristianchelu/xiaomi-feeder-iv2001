/* Tests: spec/30-processes/uart-console.md § Security (remote console password) */

#include <string.h>

#include "unity.h"

#include "config_keys.h"
#include "fake_config_port.h"
#include "remote_auth.h"

static remote_auth_result_t feed(remote_auth_t *a, const char *s)
{
    remote_auth_result_t r = REMOTE_AUTH_PENDING;

    while (*s != '\0' && r == REMOTE_AUTH_PENDING) {
        r = remote_auth_feed(a, (int)(unsigned char)*s++);
    }
    return r;
}

void test_remote_auth_not_required_without_password(void)
{
    fake_config_port_reset();
    TEST_ASSERT_FALSE(remote_auth_required());
}

void test_remote_auth_accepts_matching_line(void)
{
    remote_auth_t a;

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(PORT_OK, fake_config_port_get()->write(CONFIG_GROUP_SYSTEM, "web_pass", "chat42"));
    TEST_ASSERT_TRUE(remote_auth_required());
    remote_auth_begin(&a);
    TEST_ASSERT_EQUAL(REMOTE_AUTH_OK, feed(&a, "chat42\r"));
}

void test_remote_auth_denies_wrong_or_long_line(void)
{
    remote_auth_t a;
    char longline[64];

    fake_config_port_reset();
    TEST_ASSERT_EQUAL(PORT_OK, fake_config_port_get()->write(CONFIG_GROUP_SYSTEM, "web_pass", "chat42"));
    remote_auth_begin(&a);
    TEST_ASSERT_EQUAL(REMOTE_AUTH_DENIED, feed(&a, "chat4\r"));
    remote_auth_begin(&a);
    TEST_ASSERT_EQUAL(REMOTE_AUTH_DENIED, feed(&a, "chat42x\n"));
    memset(longline, 'a', sizeof(longline) - 1u);
    longline[sizeof(longline) - 1u] = '\0';
    remote_auth_begin(&a);
    TEST_ASSERT_EQUAL(REMOTE_AUTH_DENIED, feed(&a, longline));
}
