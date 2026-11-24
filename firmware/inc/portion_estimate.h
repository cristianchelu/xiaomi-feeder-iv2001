/*
 * Learned grams per portion — spec/30-processes/dispense-cycle.md § Portion size
 */

#ifndef PORTION_ESTIMATE_H
#define PORTION_ESTIMATE_H

#include <stdbool.h>
#include <stdint.h>

#include "weight_driver.h"

#define PORTION_ESTIMATE_DEFAULT_DG  100   /* 10 g until measured */
#define PORTION_ESTIMATE_MIN_DG       30
#define PORTION_ESTIMATE_MAX_DG      200
/* A measured per-portion sample outside this range is ignored (bowl knocked,
 * jam retries, pet eating during settle). */
#define PORTION_SAMPLE_MIN_DG         20
#define PORTION_SAMPLE_MAX_DG        300

weight_dg_t portion_estimate_dg(void);
/* Nearest whole number of portions for a gram target (at least 1). */
uint8_t portion_estimate_portions_for_dg(weight_dg_t target_dg, uint8_t max_portions);
/* Portions turned by a calibration run (one weighing over several holes). */
#define PORTION_CAL_PORTIONS          3u

typedef enum {
    PORTION_CAL_NONE = 0,   /* default 10 g, never calibrated */
    PORTION_CAL_OK,
    PORTION_CAL_FAILED,     /* last run could not be weighed / was implausible */
    PORTION_CAL_LEARNED,    /* refined from weighed feeds */
} portion_cal_status_t;

/* Result of a calibration run: delivered mass over the portions turned.
 * Sets the estimate directly (no averaging with the old value). */
bool portion_estimate_calibrate(weight_dg_t delivered_dg, uint8_t portions);
void portion_estimate_calibration_failed(void);
/* A weighed feed: delivered mass over the portions turned. Moves the estimate
 * by 1/8 of the error (dispense-cycle.md § Portion size). False = ignored. */
bool portion_estimate_learn(weight_dg_t delivered_dg, uint8_t portions);
/* Back to the 10 g default. */
void portion_estimate_reset(void);
portion_cal_status_t portion_estimate_status(void);
bool portion_estimate_is_calibrated(void);

void portion_estimate_test_reset(void);

#endif /* PORTION_ESTIMATE_H */
