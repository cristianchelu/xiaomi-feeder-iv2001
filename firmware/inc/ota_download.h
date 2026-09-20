/*
 * OTA streaming download bookkeeping — spec/30-processes/ota-flow.md
 * § Streaming download and resume
 */

#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include <stdint.h>

#include "port_err.h"

/* Consecutive attempts without a single received byte before giving up. */
#define OTA_DOWNLOAD_MAX_FAILURES 3u

typedef enum {
    OTA_DOWNLOAD_RETRY = 0,  /* reconnect with Range: bytes=<downloaded>- */
    OTA_DOWNLOAD_DONE,
    OTA_DOWNLOAD_FAILED,
} ota_download_step_t;

typedef struct {
    uint32_t downloaded;  /* bytes programmed so far == next resume offset */
    uint32_t total;       /* image length, 0 until the first response */
    unsigned failures;    /* consecutive attempts without progress */
} ota_download_state_t;

void ota_download_init(ota_download_state_t *st);

/*
 * Account for one fetch attempt: its result, the bytes it programmed, and
 * the image length it saw in the response headers (0 when none).
 */
ota_download_step_t ota_download_account(ota_download_state_t *st,
                                         port_err_t fetch_err,
                                         uint32_t received,
                                         uint32_t total_seen);

#endif /* OTA_DOWNLOAD_H */
