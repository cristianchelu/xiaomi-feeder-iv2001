/*
 * OTA streaming download bookkeeping — spec/30-processes/ota-flow.md
 * § Streaming download and resume
 */

#include <string.h>

#include "ota_download.h"

void ota_download_init(ota_download_state_t *st)
{
    memset(st, 0, sizeof(*st));
}

ota_download_step_t ota_download_account(ota_download_state_t *st,
                                         port_err_t fetch_err,
                                         uint32_t received,
                                         uint32_t total_seen)
{
    if (total_seen != 0) {
        if (st->total == 0) {
            st->total = total_seen;
        } else if (st->total != total_seen) {
            return OTA_DOWNLOAD_FAILED;
        }
    }

    st->downloaded += received;
    if (received > 0) {
        st->failures = 0;
    }

    if (st->total != 0 && st->downloaded > st->total) {
        return OTA_DOWNLOAD_FAILED;
    }
    if (st->total != 0 && st->downloaded == st->total) {
        return OTA_DOWNLOAD_DONE;
    }
    if (fetch_err == PORT_OK && st->total == 0) {
        return OTA_DOWNLOAD_FAILED;  /* clean close with no length to resume against */
    }

    st->failures++;
    return (st->failures >= OTA_DOWNLOAD_MAX_FAILURES) ? OTA_DOWNLOAD_FAILED
                                                        : OTA_DOWNLOAD_RETRY;
}
