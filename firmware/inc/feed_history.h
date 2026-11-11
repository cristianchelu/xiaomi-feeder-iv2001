/*
 * Feed history and health counters — spec/30-processes/health-monitoring.md
 */

#ifndef FEED_HISTORY_H
#define FEED_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FEED_HISTORY_DEPTH 20u

typedef struct {
    uint32_t t;        /* UTC epoch s, 0 = unknown */
    uint32_t day;      /* local YYYYMMDD, 0 = unknown */
    char src;          /* 's', 'm', 'b' */
    uint16_t want;
    int16_t got;
    bool meas;
    char res[13];      /* ok, underfill, stuck, empty_hopper, aborted, limit */
    uint8_t por;
    uint8_t pul;
    uint8_t aj;
    char why[8];
    char jam[8];
    uint16_t ok;
    uint16_t busy;
    uint16_t err;
    uint16_t ma;
    uint16_t dur;      /* s */
    bool mqtt;
    bool bowl;
} feed_record_t;

typedef enum {
    FEED_COUNTER_OK = 0,
    FEED_COUNTER_FAILED,
    FEED_COUNTER_JAMS,
    FEED_COUNTER_MQTT_LOST,
    FEED_COUNTER_BOOTS,
    FEED_COUNTER_STALLS,
    FEED_COUNTER_COUNT
} feed_counter_t;

void feed_history_add(const feed_record_t *rec);
size_t feed_history_count(void);
/* idx 0 = newest. */
bool feed_history_get(size_t idx, feed_record_t *out);
/* One record as a JSON object. Returns length or -1. */
int feed_history_format_record(const feed_record_t *rec, char *buf, size_t len);
/* All records, newest first, as a JSON array. */
int feed_history_format_json(char *buf, size_t len);
/* Grams delivered on local day `day` (sum of `got`). */
uint32_t feed_history_grams_on_day(uint32_t day);

void feed_counter_inc(feed_counter_t c);
uint32_t feed_counter_get(feed_counter_t c);

void feed_history_test_reset(void);

#endif /* FEED_HISTORY_H */
