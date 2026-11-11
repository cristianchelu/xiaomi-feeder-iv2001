/*
 * Feed history and health counters — spec/30-processes/health-monitoring.md
 *
 * Records live in NVDM group "hist" (keys f0..f19 + "next"), one key written
 * per job; a RAM copy is loaded on first use.
 */

#include "feed_history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_port.h"

#define HIST_GROUP     "hist"
#define HIST_NEXT_KEY  "next"
#define HIST_CNT_KEY   "counters"
#define HIST_LINE_MAX  128u

static feed_record_t s_recs[FEED_HISTORY_DEPTH]; /* by slot */
static bool s_valid[FEED_HISTORY_DEPTH];
static unsigned s_next;
static uint32_t s_counters[FEED_COUNTER_COUNT];
static bool s_loaded;

static void hist_key(char *key, size_t len, unsigned slot)
{
    (void)snprintf(key, len, "f%u", slot);
}

static void hist_format_line(const feed_record_t *r, char *buf, size_t len)
{
    (void)snprintf(buf, len, "%lu,%lu,%c,%u,%d,%d,%s,%u,%u,%u,%s,%s,%u,%u,%u,%u,%u,%d,%d",
                   (unsigned long)r->t, (unsigned long)r->day, r->src,
                   (unsigned)r->want, (int)r->got, r->meas ? 1 : 0, r->res,
                   (unsigned)r->por, (unsigned)r->pul, (unsigned)r->aj,
                   r->why, r->jam,
                   (unsigned)r->ok, (unsigned)r->busy, (unsigned)r->err,
                   (unsigned)r->ma, (unsigned)r->dur,
                   r->mqtt ? 1 : 0, r->bowl ? 1 : 0);
}

static bool hist_parse_line(const char *line, feed_record_t *r)
{
    unsigned long t, day;
    unsigned want, por, pul, aj, ok, busy, err, ma, dur;
    int got, meas, mqtt, bowl;
    char src;
    int n;

    memset(r, 0, sizeof(*r));
    n = sscanf(line, "%lu,%lu,%c,%u,%d,%d,%12[^,],%u,%u,%u,%7[^,],%7[^,],%u,%u,%u,%u,%u,%d,%d",
               &t, &day, &src, &want, &got, &meas, r->res, &por, &pul, &aj,
               r->why, r->jam, &ok, &busy, &err, &ma, &dur, &mqtt, &bowl);
    if (n != 19) {
        return false;
    }
    r->t = (uint32_t)t;
    r->day = (uint32_t)day;
    r->src = src;
    r->want = (uint16_t)want;
    r->got = (int16_t)got;
    r->meas = meas != 0;
    r->por = (uint8_t)por;
    r->pul = (uint8_t)pul;
    r->aj = (uint8_t)aj;
    r->ok = (uint16_t)ok;
    r->busy = (uint16_t)busy;
    r->err = (uint16_t)err;
    r->ma = (uint16_t)ma;
    r->dur = (uint16_t)dur;
    r->mqtt = mqtt != 0;
    r->bowl = bowl != 0;
    return true;
}

static void hist_load(void)
{
    const config_port_t *cfg = config_port_get();
    char line[HIST_LINE_MAX];
    char key[8];
    unsigned i;

    if (s_loaded) {
        return;
    }
    s_loaded = true;
    s_next = 0u;
    memset(s_valid, 0, sizeof(s_valid));
    memset(s_counters, 0, sizeof(s_counters));
    if (cfg == NULL) {
        return;
    }
    if (cfg->read(HIST_GROUP, HIST_NEXT_KEY, line, sizeof(line)) == PORT_OK) {
        s_next = (unsigned)strtoul(line, NULL, 10) % FEED_HISTORY_DEPTH;
    }
    for (i = 0u; i < FEED_HISTORY_DEPTH; i++) {
        hist_key(key, sizeof(key), i);
        if (cfg->read(HIST_GROUP, key, line, sizeof(line)) == PORT_OK) {
            s_valid[i] = hist_parse_line(line, &s_recs[i]);
        }
    }
    if (cfg->read(HIST_GROUP, HIST_CNT_KEY, line, sizeof(line)) == PORT_OK) {
        char *p = line;

        for (i = 0u; i < FEED_COUNTER_COUNT && *p != '\0'; i++) {
            s_counters[i] = (uint32_t)strtoul(p, &p, 10);
            if (*p == ',') {
                p++;
            }
        }
    }
}

void feed_history_add(const feed_record_t *rec)
{
    const config_port_t *cfg = config_port_get();
    char line[HIST_LINE_MAX];
    char key[8];

    if (rec == NULL) {
        return;
    }
    hist_load();
    s_recs[s_next] = *rec;
    s_valid[s_next] = true;
    if (cfg != NULL) {
        hist_format_line(rec, line, sizeof(line));
        hist_key(key, sizeof(key), s_next);
        (void)cfg->write(HIST_GROUP, key, line);
    }
    s_next = (s_next + 1u) % FEED_HISTORY_DEPTH;
    if (cfg != NULL) {
        (void)snprintf(line, sizeof(line), "%u", s_next);
        (void)cfg->write(HIST_GROUP, HIST_NEXT_KEY, line);
    }
}

size_t feed_history_count(void)
{
    size_t n = 0u;
    unsigned i;

    hist_load();
    for (i = 0u; i < FEED_HISTORY_DEPTH; i++) {
        if (s_valid[i]) {
            n++;
        }
    }
    return n;
}

bool feed_history_get(size_t idx, feed_record_t *out)
{
    size_t seen = 0u;
    unsigned k;

    hist_load();
    for (k = 1u; k <= FEED_HISTORY_DEPTH; k++) {
        unsigned slot = (s_next + FEED_HISTORY_DEPTH - k) % FEED_HISTORY_DEPTH;

        if (!s_valid[slot]) {
            continue;
        }
        if (seen == idx) {
            if (out != NULL) {
                *out = s_recs[slot];
            }
            return true;
        }
        seen++;
    }
    return false;
}

int feed_history_format_record(const feed_record_t *r, char *buf, size_t len)
{
    int n;

    if (r == NULL || buf == NULL || len == 0u) {
        return -1;
    }
    n = snprintf(buf, len,
                 "{\"t\":%lu,\"day\":%lu,\"src\":\"%c\",\"want\":%u,\"got\":%d,\"meas\":%d,"
                 "\"res\":\"%s\",\"por\":%u,\"pul\":%u,\"aj\":%u,\"why\":\"%s\",\"jam\":\"%s\","
                 "\"ok\":%u,\"busy\":%u,\"err\":%u,\"ma\":%u,\"dur\":%u,\"mqtt\":%d,\"bowl\":%d}",
                 (unsigned long)r->t, (unsigned long)r->day, r->src, (unsigned)r->want,
                 (int)r->got, r->meas ? 1 : 0, r->res, (unsigned)r->por, (unsigned)r->pul,
                 (unsigned)r->aj, r->why, r->jam, (unsigned)r->ok, (unsigned)r->busy,
                 (unsigned)r->err, (unsigned)r->ma, (unsigned)r->dur,
                 r->mqtt ? 1 : 0, r->bowl ? 1 : 0);
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    return n;
}

int feed_history_format_json(char *buf, size_t len)
{
    feed_record_t r;
    size_t off = 1u;
    size_t i;

    if (buf == NULL || len < 3u) {
        return -1;
    }
    buf[0] = '[';
    for (i = 0u; feed_history_get(i, &r); i++) {
        int n;

        if (i > 0u) {
            if (off + 1u >= len) {
                return -1;
            }
            buf[off++] = ',';
        }
        n = feed_history_format_record(&r, buf + off, len - off);
        if (n < 0) {
            return -1;
        }
        off += (size_t)n;
    }
    if (off + 2u > len) {
        return -1;
    }
    buf[off++] = ']';
    buf[off] = '\0';
    return (int)off;
}

uint32_t feed_history_grams_on_day(uint32_t day)
{
    uint32_t sum = 0u;
    unsigned i;

    if (day == 0u) {
        return 0u;
    }
    hist_load();
    for (i = 0u; i < FEED_HISTORY_DEPTH; i++) {
        if (s_valid[i] && s_recs[i].day == day && s_recs[i].got > 0) {
            sum += (uint32_t)s_recs[i].got;
        }
    }
    return sum;
}

void feed_counter_inc(feed_counter_t c)
{
    const config_port_t *cfg = config_port_get();
    char line[HIST_LINE_MAX];

    if ((unsigned)c >= FEED_COUNTER_COUNT) {
        return;
    }
    hist_load();
    s_counters[c]++;
    if (cfg != NULL) {
        (void)snprintf(line, sizeof(line), "%lu,%lu,%lu,%lu,%lu,%lu",
                       (unsigned long)s_counters[0], (unsigned long)s_counters[1],
                       (unsigned long)s_counters[2], (unsigned long)s_counters[3],
                       (unsigned long)s_counters[4], (unsigned long)s_counters[5]);
        (void)cfg->write(HIST_GROUP, HIST_CNT_KEY, line);
    }
}

uint32_t feed_counter_get(feed_counter_t c)
{
    if ((unsigned)c >= FEED_COUNTER_COUNT) {
        return 0u;
    }
    hist_load();
    return s_counters[c];
}

void feed_history_test_reset(void)
{
    s_loaded = false;
    s_next = 0u;
    memset(s_valid, 0, sizeof(s_valid));
    memset(s_counters, 0, sizeof(s_counters));
}
