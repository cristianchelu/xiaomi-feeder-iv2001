/*
 * Problem state — spec/30-processes/health-monitoring.md § Problem state
 */

#include "problem.h"

#include <stdio.h>
#include <string.h>

static volatile uint16_t s_mask;
static char s_detail[PROBLEM_CODE_MAX][PROBLEM_DETAIL_MAX];
static volatile uint32_t s_gen;

static const uint16_t s_live_mask = (1u << PROBLEM_HOPPER_EMPTY) | (1u << PROBLEM_TIME);

static const char *const s_names[PROBLEM_CODE_MAX] = {
    [PROBLEM_FEED_FAILED] = "Feed failed",
    [PROBLEM_HOPPER_EMPTY] = "Hopper empty",
    [PROBLEM_BOWL_MISSING] = "Bowl missing at feed",
    [PROBLEM_CONNECTION] = "Connection lost",
    [PROBLEM_TIME] = "Time not synced",
    [PROBLEM_DAILY_LIMIT] = "Daily limit reached",
    [PROBLEM_NOT_EATEN] = "Not eaten",
    [PROBLEM_TASK_STALL] = "Task stalled",
};

static const problem_code_t s_priority[] = {
    PROBLEM_FEED_FAILED, PROBLEM_HOPPER_EMPTY, PROBLEM_BOWL_MISSING, PROBLEM_NOT_EATEN,
    PROBLEM_DAILY_LIMIT, PROBLEM_TASK_STALL, PROBLEM_CONNECTION, PROBLEM_TIME,
};

static bool problem_valid(problem_code_t code)
{
    return code > PROBLEM_NONE && code < PROBLEM_CODE_MAX && s_names[code] != NULL;
}

static void problem_set_bit(problem_code_t code, bool on)
{
    uint16_t bit = (uint16_t)(1u << code);
    uint16_t next = on ? (uint16_t)(s_mask | bit) : (uint16_t)(s_mask & ~bit);

    if (next != s_mask) {
        s_mask = next;
        s_gen++;
    }
}

void problem_raise(problem_code_t code, const char *detail)
{
    char d[PROBLEM_DETAIL_MAX];

    if (!problem_valid(code)) {
        return;
    }
    (void)snprintf(d, sizeof(d), "%s", detail != NULL ? detail : "");
    if (strcmp(d, s_detail[code]) != 0) {
        memcpy(s_detail[code], d, sizeof(d));
        s_gen++;
    }
    problem_set_bit(code, true);
}

void problem_set_live(problem_code_t code, bool active)
{
    if (problem_valid(code)) {
        problem_set_bit(code, active);
    }
}

void problem_clear(problem_code_t code)
{
    if (problem_valid(code)) {
        problem_set_bit(code, false);
    }
}

void problem_clear_latched(void)
{
    uint16_t next = (uint16_t)(s_mask & s_live_mask);

    if (next != s_mask) {
        s_mask = next;
        s_gen++;
    }
}

bool problem_active(void)
{
    return s_mask != 0u;
}

bool problem_is_set(problem_code_t code)
{
    return problem_valid(code) && (s_mask & (1u << code)) != 0u;
}

problem_code_t problem_top(void)
{
    size_t i;

    for (i = 0u; i < sizeof(s_priority) / sizeof(s_priority[0]); i++) {
        if (problem_is_set(s_priority[i])) {
            return s_priority[i];
        }
    }
    return PROBLEM_NONE;
}

int problem_format_codes(char *buf, size_t len)
{
    size_t off = 0u;
    int c;

    if (buf == NULL || len < 3u) {
        return -1;
    }
    buf[off++] = '[';
    for (c = 1; c < PROBLEM_CODE_MAX; c++) {
        if (problem_is_set((problem_code_t)c)) {
            int n = snprintf(buf + off, len - off, "%s\"E%d\"", off > 1u ? "," : "", c);

            if (n < 0 || (size_t)n >= len - off) {
                return -1;
            }
            off += (size_t)n;
        }
    }
    if (off + 2u > len) {
        return -1;
    }
    buf[off++] = ']';
    buf[off] = '\0';
    return (int)off;
}

int problem_format_text(char *buf, size_t len)
{
    size_t off = 0u;
    int c;

    if (buf == NULL || len == 0u) {
        return -1;
    }
    buf[0] = '\0';
    for (c = 1; c < PROBLEM_CODE_MAX; c++) {
        int n;

        if (!problem_is_set((problem_code_t)c)) {
            continue;
        }
        n = snprintf(buf + off, len - off, s_detail[c][0] != '\0' ? "%s%s (%s)" : "%s%s",
                     off > 0u ? "; " : "", s_names[c], s_detail[c]);
        if (n < 0 || (size_t)n >= len - off) {
            return (int)off;
        }
        off += (size_t)n;
    }
    return (int)off;
}

uint32_t problem_generation(void)
{
    return s_gen;
}

void problem_test_reset(void)
{
    s_mask = 0u;
    memset(s_detail, 0, sizeof(s_detail));
    s_gen++;
}
