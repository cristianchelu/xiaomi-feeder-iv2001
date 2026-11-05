/*
 * Eaten-today tracking — spec/30-processes/eaten-today.md
 */

#include "eaten_today.h"

#include <stdio.h>
#include <stdlib.h>

#include "config_keys.h"
#include "config_port.h"
#include "time_local.h"

static weight_dg_t s_total_dg;
static uint32_t s_day;
static weight_dg_t s_ref_dg;
static bool s_ref_valid;
static weight_dg_t s_last_dg;
static unsigned s_settle;

static void eaten_today_persist(void)
{
    const config_port_t *cfg = config_port_get();
    char buf[16];

    if (cfg == NULL) {
        return;
    }
    (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_day);
    (void)cfg->write(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_DAY, buf);
    (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_total_dg);
    (void)cfg->write(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_DG, buf);
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned)EATEN_TODAY_SCHEMA_VERSION);
    (void)cfg->write(CONFIG_GROUP_EATEN, CONFIG_KEY_EATEN_VER, buf);
}

static bool eaten_today_read_u32(const char *key, uint32_t *out)
{
    const config_port_t *cfg = config_port_get();
    char buf[16];
    char *end = NULL;
    unsigned long v;

    if (cfg == NULL || cfg->read(CONFIG_GROUP_EATEN, key, buf, sizeof(buf)) != PORT_OK) {
        return false;
    }
    v = strtoul(buf, &end, 10);
    if (end == buf || *end != '\0') {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

void eaten_today_init(uint32_t day_key)
{
    uint32_t day = 0u;
    uint32_t dg = 0u;

    uint32_t ver = 0u;

    eaten_today_test_reset();
    if (eaten_today_read_u32(CONFIG_KEY_EATEN_DAY, &day) &&
        (!eaten_today_read_u32(CONFIG_KEY_EATEN_VER, &ver) ||
         ver != EATEN_TODAY_SCHEMA_VERSION)) {
        /* Totals written by an older firmware may include calibration artefacts. */
        s_day = day_key;
        s_total_dg = 0;
        eaten_today_persist();
        return;
    }
    day = 0u;
    if (!eaten_today_read_u32(CONFIG_KEY_EATEN_DAY, &day) ||
        !eaten_today_read_u32(CONFIG_KEY_EATEN_DG, &dg)) {
        s_day = day_key;
        return;
    }
    if (day_key != 0u && day != 0u && day != day_key) {
        s_day = day_key;
        s_total_dg = 0;
        eaten_today_persist();
        return;
    }
    s_day = (day != 0u) ? day : day_key;
    s_total_dg = (weight_dg_t)dg;
}

static void eaten_today_rollover(uint32_t day_key)
{
    if (day_key == 0u) {
        return;
    }
    if (s_day == 0u) {
        s_day = day_key;
        eaten_today_persist();
    } else if (s_day != day_key) {
        s_day = day_key;
        s_total_dg = 0;
        eaten_today_persist();
    }
}

void eaten_today_reset(void)
{
    s_total_dg = 0;
    s_ref_valid = false;
    s_settle = 0u;
    eaten_today_persist();
}

void eaten_today_invalidate(void)
{
    s_ref_valid = false;
    s_settle = 0u;
}

void eaten_today_on_idle_sample(weight_dg_t presented_dg, bool usable, uint32_t day_key)
{
    weight_dg_t delta;

    eaten_today_rollover(day_key);

    if (!usable) {
        eaten_today_invalidate();
        return;
    }
    if (!s_ref_valid) {
        s_ref_dg = presented_dg;
        s_ref_valid = true;
        s_last_dg = presented_dg;
        s_settle = 0u;
        return;
    }

    delta = presented_dg - s_last_dg;
    if (delta < 0) {
        delta = -delta;
    }
    s_last_dg = presented_dg;
    if (delta <= EATEN_TODAY_SETTLE_BAND_DG) {
        if (s_settle < EATEN_TODAY_SETTLE_SAMPLES) {
            s_settle++;
        }
    } else {
        s_settle = 0u;
    }
    if (s_settle < EATEN_TODAY_SETTLE_SAMPLES) {
        return;
    }

    if (presented_dg > s_ref_dg) {
        s_ref_dg = presented_dg;
    } else if (s_ref_dg - presented_dg >= EATEN_TODAY_MIN_EATEN_DG) {
        s_total_dg += s_ref_dg - presented_dg;
        s_ref_dg = presented_dg;
        eaten_today_persist();
    }
}

weight_dg_t eaten_today_total_dg(void)
{
    return s_total_dg;
}

uint32_t eaten_today_day_key_now(void)
{
    time_local_t t;

    if (!time_local_now(&t)) {
        return 0u;
    }
    return (uint32_t)t.year * 10000u + (uint32_t)t.month * 100u + (uint32_t)t.day;
}

void eaten_today_test_reset(void)
{
    s_total_dg = 0;
    s_day = 0u;
    s_ref_dg = 0;
    s_ref_valid = false;
    s_last_dg = 0;
    s_settle = 0u;
}
