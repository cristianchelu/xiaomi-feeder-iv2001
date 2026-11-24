/*
 * Eaten-today tracking — spec/30-processes/eaten-today.md
 */

#ifndef EATEN_TODAY_H
#define EATEN_TODAY_H

#include <stdbool.h>
#include <stdint.h>

#include "weight_units.h"

/* The CS1270 reads whole grams (1 count = 10 dg) and flickers by one count. */
#define EATEN_TODAY_SETTLE_SAMPLES  6u   /* [tune] window, 3 s at 500 ms */
#define EATEN_TODAY_SETTLE_BAND_DG  10   /* [tune] max-min inside the window */
#define EATEN_TODAY_MIN_EATEN_DG    30   /* [tune] counted once 3 g below the reference */

/* Restore from NVDM. day_key = local YYYYMMDD, 0 when the clock is unknown. */
void eaten_today_init(uint32_t day_key);
void eaten_today_on_idle_sample(weight_dg_t presented_dg, bool usable, uint32_t day_key);
/* Stored-format version: a mismatch (new firmware) zeroes the stored total once. */
#define EATEN_TODAY_SCHEMA_VERSION  2u
/* Zero today's total now (HA button / web). */
void eaten_today_reset(void);
/* Dispense completed or bowl removed: next settled sample rebases. */
void eaten_today_invalidate(void);
weight_dg_t eaten_today_total_dg(void);
/* Local YYYYMMDD or 0 when the clock is unknown. */
uint32_t eaten_today_day_key_now(void);

void eaten_today_test_reset(void);

#endif /* EATEN_TODAY_H */
