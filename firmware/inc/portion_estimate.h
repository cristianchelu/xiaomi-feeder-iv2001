/*
 * Learned grams per portion — spec/30-processes/dispense-cycle.md § Portion size
 */

#ifndef PORTION_ESTIMATE_H
#define PORTION_ESTIMATE_H

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
/* Learn from a measured dispense: EMA (1/4) of delivered/portions, persisted. */
void portion_estimate_learn(weight_dg_t delivered_dg, uint8_t portions);

void portion_estimate_test_reset(void);

#endif /* PORTION_ESTIMATE_H */
