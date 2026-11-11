/*
 * Learned grams per portion — spec/30-processes/dispense-cycle.md § Portion size
 *
 * The auger delivers a kibble-dependent mass per index hole (~8.5 g measured
 * on the reference unit vs the 10 g default). Refined by every weighed feed
 * (1/8 of the error) and set directly by a calibration run.
 */

#include "portion_estimate.h"

#include <stdio.h>
#include <stdlib.h>

#include "app_log.h"
#include "config_keys.h"
#include "config_port.h"

#define PORTION_KEY  "portion_dg"

static weight_dg_t s_est_dg;
static int s_loaded;
static portion_cal_status_t s_status;
static int32_t s_acc;            /* estimate x 8 (learning accumulator) */
static weight_dg_t s_stored_dg;  /* value in NVDM */

#define PORTION_LEARN_SHIFT      3     /* [tune] 1/8 of the error */
#define PORTION_LEARN_STORE_DG   2     /* [tune] NVDM write threshold */

static void portion_estimate_load(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[12];

    if (s_loaded) {
        return;
    }
    s_loaded = 1;
    s_est_dg = PORTION_ESTIMATE_DEFAULT_DG;
    if (cfg != NULL && cfg->read(CONFIG_GROUP_FEED, PORTION_KEY, buf, sizeof(buf)) == PORT_OK) {
        long v = strtol(buf, NULL, 10);

        if (v >= PORTION_ESTIMATE_MIN_DG && v <= PORTION_ESTIMATE_MAX_DG) {
            s_est_dg = (weight_dg_t)v;
            if (s_est_dg != PORTION_ESTIMATE_DEFAULT_DG) {
                s_status = PORTION_CAL_OK;
            }
        }
    }
    s_stored_dg = s_est_dg;
    s_acc = (int32_t)s_est_dg << PORTION_LEARN_SHIFT;
}

weight_dg_t portion_estimate_dg(void)
{
    portion_estimate_load();
    return s_est_dg;
}

uint8_t portion_estimate_portions_for_dg(weight_dg_t target_dg, uint8_t max_portions)
{
    weight_dg_t est = portion_estimate_dg();
    int32_t n;

    if (target_dg <= 0) {
        return 1u;
    }
    n = (target_dg + est / 2) / est;
    if (n < 1) {
        n = 1;
    }
    if (max_portions > 0u && n > (int32_t)max_portions) {
        n = max_portions;
    }
    return (uint8_t)n;
}

static void portion_estimate_store(weight_dg_t dg)
{
    const config_port_t *cfg = config_port_get();
    char buf[12];

    s_est_dg = dg;
    s_stored_dg = dg;
    s_acc = (int32_t)dg << PORTION_LEARN_SHIFT;
    if (cfg != NULL) {
        (void)snprintf(buf, sizeof(buf), "%ld", (long)dg);
        (void)cfg->write(CONFIG_GROUP_FEED, PORTION_KEY, buf);
    }
}

bool portion_estimate_calibrate(weight_dg_t delivered_dg, uint8_t portions)
{
    weight_dg_t sample;

    portion_estimate_load();
    if (portions == 0u || delivered_dg <= 0) {
        portion_estimate_calibration_failed();
        return false;
    }
    sample = (weight_dg_t)((delivered_dg + (weight_dg_t)portions / 2) / (weight_dg_t)portions);
    if (sample < PORTION_ESTIMATE_MIN_DG || sample > PORTION_ESTIMATE_MAX_DG) {
        app_log_info("dispense", "portion calibration rejected: %ld dg over %u portions",
                     (long)delivered_dg, (unsigned)portions);
        portion_estimate_calibration_failed();
        return false;
    }
    portion_estimate_store(sample);
    s_status = PORTION_CAL_OK;
    app_log_info("dispense", "portion calibrated: %ld dg (%ld dg over %u portions)",
                 (long)sample, (long)delivered_dg, (unsigned)portions);
    return true;
}

bool portion_estimate_learn(weight_dg_t delivered_dg, uint8_t portions)
{
    weight_dg_t sample;
    weight_dg_t est;

    portion_estimate_load();
    if (portions == 0u || delivered_dg <= 0) {
        return false;
    }
    sample = (weight_dg_t)((delivered_dg + (weight_dg_t)portions / 2) / (weight_dg_t)portions);
    if (sample < PORTION_SAMPLE_MIN_DG || sample > PORTION_SAMPLE_MAX_DG) {
        return false;
    }
    s_acc += (int32_t)sample - ((s_acc + (1 << (PORTION_LEARN_SHIFT - 1))) >> PORTION_LEARN_SHIFT);
    est = (weight_dg_t)((s_acc + (1 << (PORTION_LEARN_SHIFT - 1))) >> PORTION_LEARN_SHIFT);
    if (est < PORTION_ESTIMATE_MIN_DG) {
        est = PORTION_ESTIMATE_MIN_DG;
        s_acc = (int32_t)est << PORTION_LEARN_SHIFT;
    }
    if (est > PORTION_ESTIMATE_MAX_DG) {
        est = PORTION_ESTIMATE_MAX_DG;
        s_acc = (int32_t)est << PORTION_LEARN_SHIFT;
    }
    s_est_dg = est;
    s_status = PORTION_CAL_LEARNED;
    app_log_info("dispense", "portion learned %ld dg -> estimate %ld dg",
                 (long)sample, (long)est);
    if (est - s_stored_dg >= PORTION_LEARN_STORE_DG || s_stored_dg - est >= PORTION_LEARN_STORE_DG) {
        const config_port_t *cfg = config_port_get();
        char buf[12];

        s_stored_dg = est;
        if (cfg != NULL) {
            (void)snprintf(buf, sizeof(buf), "%ld", (long)est);
            (void)cfg->write(CONFIG_GROUP_FEED, PORTION_KEY, buf);
        }
    }
    return true;
}

void portion_estimate_calibration_failed(void)
{
    s_status = PORTION_CAL_FAILED;
}

void portion_estimate_reset(void)
{
    portion_estimate_load();
    portion_estimate_store(PORTION_ESTIMATE_DEFAULT_DG);
    s_status = PORTION_CAL_NONE;
    app_log_info("dispense", "portion size reset to default");
}

portion_cal_status_t portion_estimate_status(void)
{
    portion_estimate_load();
    return s_status;
}

bool portion_estimate_is_calibrated(void)
{
    return portion_estimate_dg() != PORTION_ESTIMATE_DEFAULT_DG;
}

void portion_estimate_test_reset(void)
{
    s_status = PORTION_CAL_NONE;
    s_loaded = 0;
    s_est_dg = PORTION_ESTIMATE_DEFAULT_DG;
    s_stored_dg = PORTION_ESTIMATE_DEFAULT_DG;
    s_acc = (int32_t)PORTION_ESTIMATE_DEFAULT_DG << PORTION_LEARN_SHIFT;
}
