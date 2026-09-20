/*
 * OTA port adapter — one HTTP GET streamed into the inactive bank, resumed
 * with a Range request when the connection drops (ota-flow.md § Streaming
 * download and resume).
 */

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "task_def.h"

#include "app_log.h"

#include "httpclient.h"

#include "flash_bank_logic.h"
#include "flash_bank_port.h"
#include "hal_cache.h"
#include "hal_sys.h"
#include "ota_download.h"
#include "ota_image.h"
#include "ota_port.h"
#include "ota_preflight.h"
#include "ota_verify.h"
#include "wifi_api.h"

#define OTA_DL_TASK_STACK   (12288)
#define OTA_DL_TASK_PRIO    (TASK_PRIORITY_ABOVE_NORMAL)  /* above app, below lwIP/net */
#define OTA_HDR_BUF         512
#define OTA_SETTLE_MS       500   /* ota-flow.md § Internal progress phases */

typedef struct {
    char url[OTA_URL_MAX_LEN + 1];
    uint8_t expected_sha512[FLASH_BANK_SHA512_LEN];
    bool has_expected_sha512;
} ota_job_t;

static TaskHandle_t s_ota_task;
static volatile ota_status_t s_status = OTA_STATUS_IDLE;
static volatile bool s_abort_requested;
static ota_progress_cb_t s_progress_cb;
static void *s_progress_ctx;
static uint8_t s_image_hash[FLASH_BANK_SHA512_LEN];
static uint8_t s_last_report_pct;
static TickType_t s_flash_ticks;
static TickType_t s_recv_ticks;
static uint32_t s_recv_calls;

static void ota_adapter_report(ota_status_t status, uint8_t pct, const char *error)
{
    ota_progress_t progress;

    s_status = status;
    progress.status = status;
    progress.pct = pct;
    progress.error = error;

    if (s_progress_cb != NULL) {
        s_progress_cb(&progress, s_progress_ctx);
    }
}

static void ota_adapter_reboot(void)
{
    /* Deauth from the AP, then WDT reboot into the inactive bank. */
    APP_LOG_I("ota", "pre-reboot: disconnect AP");
    (void)wifi_connection_disconnect_ap();
    vTaskDelay(pdMS_TO_TICKS(500));

    hal_cache_disable();
    hal_cache_deinit();
    hal_sys_reboot(HAL_SYS_REBOOT_MAGIC, WHOLE_SYSTEM_REBOOT_COMMAND);
}

/*
 * Parse total file size from a Content-Range header value.
 * Expected format: "bytes 163840-525623/525624" → returns 525624.
 */
static uint32_t ota_adapter_parse_content_range_total(const char *hdr_buf)
{
    int val_pos = 0;
    int val_len = 0;
    const char *p;
    const char *end;

    if (hdr_buf == NULL) {
        return 0;
    }

    if (httpclient_get_response_header_value((char *)hdr_buf,
                                            "Content-Range",
                                            &val_pos, &val_len) != 0) {
        return 0;
    }

    p = hdr_buf + val_pos;
    end = p + val_len;

    while (p < end && *p != '/') {
        p++;
    }
    if (p >= end) {
        return 0;
    }

    return strtoul(p + 1, NULL, 10);
}

static void ota_adapter_progress(uint32_t downloaded, uint32_t total)
{
    uint8_t pct;

    if (total == 0) {
        return;
    }

    pct = ota_progress_pct(downloaded, total);
    if (pct >= s_last_report_pct + OTA_PROGRESS_STEP_PCT || pct == 100) {
        app_log_debug("ota",
                      "%u%% (%lu/%lu) heap=%u min=%u",
                      pct,
                      (unsigned long)downloaded,
                      (unsigned long)total,
                      (unsigned)xPortGetFreeHeapSize(),
                      (unsigned)xPortGetMinimumEverFreeHeapSize());
        s_last_report_pct = pct;
        ota_adapter_report(OTA_STATUS_DOWNLOADING, pct, "");
    }
}

/*
 * One GET streamed into the inactive bank, starting at `offset` (a Range
 * resume when offset > 0).  Bytes programmed by this attempt are reported in
 * *received_out even on failure; *total_out is the image length learned from
 * the response headers (0 when none).
 */
static port_err_t ota_adapter_stream(const char *url,
                                     char *chunk_buf,
                                     char *hdr_buf,
                                     uint32_t offset,
                                     uint32_t known_total,
                                     uint32_t *total_out,
                                     uint32_t *received_out)
{
    httpclient_t client = {0};
    httpclient_data_t client_data = {0};
    char range_hdr[48];
    int32_t ret;
    int32_t recv_temp = -1;
    uint32_t received = 0;
    uint32_t total = 0;
    bool headers_checked = false;
    const flash_bank_port_t *flash = flash_bank_port_get();

    *received_out = 0;
    *total_out = 0;

    client_data.response_buf = chunk_buf;
    client_data.response_buf_len = OTA_CHUNK_SIZE;
    client_data.header_buf = hdr_buf;
    client_data.header_buf_len = OTA_HDR_BUF;

    if (offset > 0) {
        snprintf(range_hdr, sizeof(range_hdr),
                 "Range: bytes=%lu-\r\n", (unsigned long)offset);
        httpclient_set_custom_header(&client, range_hdr);
    }

    ret = httpclient_connect(&client, (char *)url);
    if (ret != HTTPCLIENT_OK) {
        app_log_error("ota", "connect fail at %lu", (unsigned long)offset);
        httpclient_close(&client);
        return PORT_ERR_IO;
    }

    {
        struct timeval tv = { .tv_sec = 20, .tv_usec = 0 };

        (void)setsockopt(client.socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    ret = httpclient_send_request(&client, (char *)url, HTTPCLIENT_GET, &client_data);
    if (ret != HTTPCLIENT_OK) {
        app_log_error("ota", "send fail at %lu", (unsigned long)offset);
        httpclient_close(&client);
        return PORT_ERR_IO;
    }

    /*
     * First call parses the status line and headers and returns the first
     * body bytes; the remainder is read with plain recv() in OTA_CHUNK_SIZE
     * pieces (one socket call and one window credit per chunk instead of the
     * HTTP client's 1-byte-then-1 KB pattern).
     */
    do {
        uint32_t data_len;

        if (s_abort_requested) {
            httpclient_close(&client);
            *received_out = received;
            return PORT_ERR_BUSY;
        }

        if (headers_checked) {
            uint32_t want = total - offset - received;
            int n;
            TickType_t t_recv;

            if (want > OTA_CHUNK_SIZE) {
                want = OTA_CHUNK_SIZE;
            }

            t_recv = xTaskGetTickCount();
            n = recv(client.socket, chunk_buf, (size_t)want, 0);
            s_recv_ticks += xTaskGetTickCount() - t_recv;
            s_recv_calls++;

            if (n <= 0) {
                app_log_error("ota",
                              "recv fail at %lu+%lu ret=%d",
                              (unsigned long)offset,
                              (unsigned long)received,
                              n);
                httpclient_close(&client);
                *received_out = received;
                return PORT_ERR_IO;
            }

            data_len = (uint32_t)n;
            ret = (offset + received + data_len < total) ? HTTPCLIENT_RETRIEVE_MORE_DATA
                                                         : HTTPCLIENT_OK;
            goto program;
        }

        {
            TickType_t t_recv = xTaskGetTickCount();

            ret = httpclient_recv_response(&client, &client_data);
            s_recv_ticks += xTaskGetTickCount() - t_recv;
            s_recv_calls++;
        }
        if (ret < HTTPCLIENT_OK) {
            app_log_error("ota",
                          "recv fail at %lu+%lu ret=%ld",
                          (unsigned long)offset,
                          (unsigned long)received,
                          (long)ret);
            httpclient_close(&client);
            *received_out = received;
            return PORT_ERR_IO;
        }

        if (!headers_checked) {
            int code = httpclient_get_response_code(&client);

            headers_checked = true;

            if (offset == 0 && code == 200) {
                total = (client_data.response_content_len > 0)
                        ? (uint32_t)client_data.response_content_len : 0;
            } else if (code == 206) {
                total = ota_adapter_parse_content_range_total(hdr_buf);
            } else {
                app_log_error("ota", "server returned %d at %lu", code, (unsigned long)offset);
                httpclient_close(&client);
                return PORT_ERR_IO;
            }

            if (total == 0) {
                app_log_error("ota", "image length missing");
                httpclient_close(&client);
                return PORT_ERR_IO;
            }

            *total_out = total;

            if (known_total != 0 && total != known_total) {
                app_log_error("ota", "image length changed %lu -> %lu",
                              (unsigned long)known_total, (unsigned long)total);
                httpclient_close(&client);
                return PORT_ERR_IO;
            }

            if (!ota_image_size_allowed(total)) {
                app_log_error("ota", "image length %lu exceeds bank", (unsigned long)total);
                httpclient_close(&client);
                return PORT_ERR_INVALID_ARG;
            }

            if (offset == 0) {
                app_log_debug("ota", "file size %lu bytes", (unsigned long)total);
            }
        }

        if (recv_temp < 0) {
            recv_temp = client_data.response_content_len;
        }

        data_len = (recv_temp >= 0)
                   ? (uint32_t)(recv_temp - client_data.retrieve_len) : 0;
        recv_temp = client_data.retrieve_len;

program:
        if (data_len > 0) {
            uint32_t write_offset = offset + received;
            TickType_t t0;

            if (write_offset + data_len > CM4_LENGTH) {
                httpclient_close(&client);
                *received_out = received;
                return PORT_ERR_INVALID_ARG;
            }

            t0 = xTaskGetTickCount();
            if (flash->write_inactive(write_offset, (const uint8_t *)chunk_buf,
                                      data_len) != PORT_OK) {
                app_log_error("ota",
                              "flash write fail at %lu len=%lu",
                              (unsigned long)write_offset,
                              (unsigned long)data_len);
                httpclient_close(&client);
                *received_out = received;
                return PORT_ERR_IO;
            }
            s_flash_ticks += xTaskGetTickCount() - t0;

            received += data_len;
            ota_adapter_progress(offset + received, total);
        }
    } while (ret == HTTPCLIENT_RETRIEVE_MORE_DATA);

    httpclient_close(&client);
    *received_out = received;
    return PORT_OK;
}

/*
 * Streaming download with Range resume (ota_download.c holds the rules).
 * No hash is kept over the received stream: verification hashes the bank
 * afterwards (spec/30-processes/ota-flow.md § Verification).
 */
static port_err_t ota_adapter_http_download(const char *url,
                                            uint32_t *downloaded_out)
{
    /* Task-stack buffers: no heap (fragmented) and no permanent BSS reservation. */
    uint8_t chunk_storage[OTA_CHUNK_SIZE];
    char hdr_storage[OTA_HDR_BUF];
    ota_download_state_t st;
    TickType_t t_start = xTaskGetTickCount();

    ota_download_init(&st);
    s_last_report_pct = 0;
    s_flash_ticks = 0;
    s_recv_ticks = 0;
    s_recv_calls = 0;

    flash_bank_port_get()->erase_inactive();

    app_log_debug("ota",
                  "download start heap=%u min=%u",
                  (unsigned)xPortGetFreeHeapSize(),
                  (unsigned)xPortGetMinimumEverFreeHeapSize());

    for (;;) {
        uint32_t total_seen = 0;
        uint32_t received = 0;
        port_err_t err;
        ota_download_step_t step;

        if (s_abort_requested) {
            return PORT_ERR_BUSY;
        }

        err = ota_adapter_stream(url, (char *)chunk_storage, hdr_storage,
                                 st.downloaded, st.total, &total_seen, &received);
        if (err == PORT_ERR_BUSY || err == PORT_ERR_INVALID_ARG) {
            return err;
        }

        step = ota_download_account(&st, err, received, total_seen);
        if (step == OTA_DOWNLOAD_DONE) {
            break;
        }
        if (step == OTA_DOWNLOAD_FAILED) {
            app_log_error("ota", "download failed at %lu after %u attempts",
                          (unsigned long)st.downloaded, st.failures);
            return PORT_ERR_IO;
        }

        app_log_debug("ota", "retry %u at %lu", st.failures, (unsigned long)st.downloaded);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!ota_image_size_allowed(st.downloaded)) {
        return PORT_ERR_INVALID_ARG;
    }

    *downloaded_out = st.downloaded;
    app_log_info("ota", "download complete bytes=%lu in %lu ms flash=%lu ms recv=%lu ms/%lu",
                 (unsigned long)st.downloaded,
                 (unsigned long)((xTaskGetTickCount() - t_start) * portTICK_PERIOD_MS),
                 (unsigned long)(s_flash_ticks * portTICK_PERIOD_MS),
                 (unsigned long)(s_recv_ticks * portTICK_PERIOD_MS),
                 (unsigned long)s_recv_calls);
    return PORT_OK;
}

static void ota_adapter_task_fail(const char *error)
{
    ota_preflight_resume_idle_tasks();
    ota_adapter_report(OTA_STATUS_ERROR, 0, error);
    s_status = OTA_STATUS_IDLE;
    s_ota_task = NULL;
    vTaskDelete(NULL);
}

static void ota_adapter_task(void *param)
{
    ota_job_t job;
    const flash_bank_port_t *flash = flash_bank_port_get();
    uint32_t downloaded = 0;
    port_err_t err;

    (void)param;

    memcpy(&job, param, sizeof(job));
    vPortFree(param);

    s_abort_requested = false;
    app_log_info("ota", "task start url=%s sha512=%s", job.url, job.has_expected_sha512 ? "yes" : "no");
    ota_adapter_report(OTA_STATUS_PREPARING, 0, "");

    vTaskDelay(pdMS_TO_TICKS(OTA_SETTLE_MS));
    app_log_info("ota", "mqtt down, http start");
    ota_adapter_report(OTA_STATUS_CONNECTING, 0, "");

    err = ota_adapter_http_download(job.url, &downloaded);
    if (err != PORT_OK) {
        const char *error = (err == PORT_ERR_INVALID_ARG) ? "image_too_large" : "download_failed";
        app_log_error("ota", "download error=%s err=%d", error, (int)err);
        ota_adapter_task_fail(error);
        return;
    }

    ota_adapter_report(OTA_STATUS_VERIFYING, 100, "");

    {
        boot_bank_t inactive = flash_bank_inactive(flash->get_active_bank());

        if (ota_image_check_vector_table_in_bank(flash_bank_rom_offset(inactive)) != PORT_OK) {
            app_log_error("ota", "vector table not found in bank");
            ota_adapter_task_fail("verify_failed");
            return;
        }
    }

    err = ota_verify_bank(flash, downloaded,
                          job.has_expected_sha512 ? job.expected_sha512 : NULL,
                          s_image_hash);
    if (err == PORT_ERR_INVALID_ARG) {
        app_log_error("ota", "sha512 mismatch");
        ota_adapter_task_fail("verify_failed");
        return;
    }
    if (err != PORT_OK) {
        app_log_error("ota", "flash verify failed err=%d", (int)err);
        ota_adapter_task_fail("verify_failed");
        return;
    }

    ota_adapter_report(OTA_STATUS_APPLYING, 100, "");
    app_log_info("ota", "bank swap pending");

    if (flash->swap_banks(s_image_hash) != PORT_OK) {
        app_log_error("ota", "bank swap failed");
        ota_adapter_task_fail("download_failed");
        return;
    }

    ota_adapter_reboot();
}

static port_err_t ota_adapter_start(const char *url,
                                    const uint8_t *expected_sha512,
                                    bool has_expected_sha512)
{
    ota_job_t *job;

    if (url == NULL || url[0] == '\0') {
        return PORT_ERR_INVALID_ARG;
    }

    if (s_ota_task != NULL || s_status != OTA_STATUS_IDLE) {
        app_log_error("ota", "start busy status=%d", (int)s_status);
        return PORT_ERR_BUSY;
    }

    job = pvPortMalloc(sizeof(*job));
    if (job == NULL) {
        return PORT_ERR_IO;
    }

    strncpy(job->url, url, sizeof(job->url) - 1);
    job->url[sizeof(job->url) - 1] = '\0';
    job->has_expected_sha512 = has_expected_sha512;
    if (has_expected_sha512 && expected_sha512 != NULL) {
        memcpy(job->expected_sha512, expected_sha512, FLASH_BANK_SHA512_LEN);
    }

    ota_preflight_suspend_idle_tasks();

    if (xTaskCreate(ota_adapter_task,
                    "ota_dl",
                    OTA_DL_TASK_STACK / sizeof(StackType_t),
                    job,
                    OTA_DL_TASK_PRIO,
                    &s_ota_task) != pdPASS) {
        app_log_error("ota",
                      "task create failed heap free=%u min=%u need_stack=%u",
                      (unsigned)xPortGetFreeHeapSize(),
                      (unsigned)xPortGetMinimumEverFreeHeapSize(),
                      (unsigned)OTA_DL_TASK_STACK);
        ota_preflight_resume_idle_tasks();
        vPortFree(job);
        s_status = OTA_STATUS_IDLE;
        return PORT_ERR_IO;
    }

    app_log_info("ota", "task created url=%s", url);
    return PORT_OK;
}

static ota_status_t ota_adapter_get_status(void)
{
    return s_status;
}

static port_err_t ota_adapter_abort(void)
{
    if (s_ota_task == NULL) {
        return PORT_ERR_INVALID_ARG;
    }

    s_abort_requested = true;
    return PORT_OK;
}

static void ota_adapter_set_progress_cb(ota_progress_cb_t cb, void *ctx)
{
    s_progress_cb = cb;
    s_progress_ctx = ctx;
}

static const ota_port_t s_ota_port = {
    .start = ota_adapter_start,
    .get_status = ota_adapter_get_status,
    .abort = ota_adapter_abort,
    .set_progress_cb = ota_adapter_set_progress_cb,
};

const ota_port_t *ota_port_get(void)
{
    return &s_ota_port;
}
