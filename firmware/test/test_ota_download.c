/* Tests: spec/30-processes/ota-flow.md § Streaming download and resume */

#include "unity.h"

#include "ota_download.h"

#define TOTAL 525624u

void test_ota_download_complete_in_one_fetch(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_DONE, ota_download_account(&st, PORT_OK, TOTAL, TOTAL));
    TEST_ASSERT_EQUAL_UINT32(TOTAL, st.downloaded);
    TEST_ASSERT_EQUAL_UINT32(TOTAL, st.total);
}

void test_ota_download_partial_then_error_resumes_at_offset(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_RETRY, ota_download_account(&st, PORT_ERR_IO, 163840u, TOTAL));
    TEST_ASSERT_EQUAL_UINT32(163840u, st.downloaded);
    TEST_ASSERT_EQUAL_UINT(1, st.failures);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_DONE,
                      ota_download_account(&st, PORT_OK, TOTAL - 163840u, TOTAL));
    TEST_ASSERT_EQUAL_UINT32(TOTAL, st.downloaded);
}

void test_ota_download_three_failures_without_progress_fail(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_RETRY, ota_download_account(&st, PORT_ERR_IO, 0, 0));
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_RETRY, ota_download_account(&st, PORT_ERR_IO, 0, 0));
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_FAILED, ota_download_account(&st, PORT_ERR_IO, 0, 0));
    TEST_ASSERT_EQUAL_UINT(3, st.failures);
}

void test_ota_download_progress_resets_failure_count(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    (void)ota_download_account(&st, PORT_ERR_IO, 0, 0);
    (void)ota_download_account(&st, PORT_ERR_IO, 0, 0);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_RETRY, ota_download_account(&st, PORT_ERR_IO, 4096u, TOTAL));
    TEST_ASSERT_EQUAL_UINT(1, st.failures);
    TEST_ASSERT_EQUAL_UINT32(4096u, st.downloaded);
}

void test_ota_download_clean_close_before_total_is_retry(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_RETRY, ota_download_account(&st, PORT_OK, 1000u, TOTAL));
    TEST_ASSERT_EQUAL_UINT(1, st.failures);
}

void test_ota_download_total_change_fails(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    (void)ota_download_account(&st, PORT_ERR_IO, 100u, 500u);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_FAILED, ota_download_account(&st, PORT_ERR_IO, 100u, 600u));
}

void test_ota_download_overrun_fails(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_FAILED, ota_download_account(&st, PORT_OK, 600u, 500u));
}

void test_ota_download_clean_close_without_length_fails(void)
{
    ota_download_state_t st;

    ota_download_init(&st);
    TEST_ASSERT_EQUAL(OTA_DOWNLOAD_FAILED, ota_download_account(&st, PORT_OK, 500u, 0));
}
