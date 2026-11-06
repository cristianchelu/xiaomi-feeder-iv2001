/*
 * Learned grams per portion — spec/30-processes/dispense-cycle.md § Portion size
 *
 * The auger delivers a kibble-dependent mass per index hole (~8.5 g measured
 * on the reference unit vs the 10 g default). Measured dispenses refine it.
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
        }
    }
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

void portion_estimate_learn(weight_dg_t delivered_dg, uint8_t portions)
{
    const config_port_t *cfg = config_port_get();
    weight_dg_t sample;
    weight_dg_t next;
    char buf[12];

    if (portions == 0u || delivered_dg <= 0) {
        return;
    }
    sample = (weight_dg_t)(delivered_dg / (weight_dg_t)portions);
    if (sample < PORTION_SAMPLE_MIN_DG || sample > PORTION_SAMPLE_MAX_DG) {
        return;
    }

    portion_estimate_load();
    {
        weight_dg_t diff = sample - s_est_dg;
        weight_dg_t step = (diff + (diff > 0 ? 2 : -2)) / 4;   /* EMA 1/4, rounded */

        if (step == 0 && diff != 0) {
            step = diff > 0 ? 1 : -1;
        }
        next = s_est_dg + step;
    }
    if (next < PORTION_ESTIMATE_MIN_DG) {
        next = PORTION_ESTIMATE_MIN_DG;
    }
    if (next > PORTION_ESTIMATE_MAX_DG) {
        next = PORTION_ESTIMATE_MAX_DG;
    }
    if (next == s_est_dg) {
        return;
    }
    s_est_dg = next;
    app_log_info("dispense", "portion size %ld dg (sample %ld dg)", (long)next, (long)sample);
    if (cfg != NULL) {
        (void)snprintf(buf, sizeof(buf), "%ld", (long)next);
        (void)cfg->write(CONFIG_GROUP_FEED, PORTION_KEY, buf);
    }
}

void portion_estimate_test_reset(void)
{
    s_loaded = 0;
    s_est_dg = PORTION_ESTIMATE_DEFAULT_DG;
}
