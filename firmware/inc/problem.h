/*
 * Problem state — spec/30-processes/health-monitoring.md § Problem state
 */

#ifndef PROBLEM_H
#define PROBLEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    PROBLEM_NONE = 0,
    PROBLEM_FEED_FAILED = 1,     /* E1 latched */
    PROBLEM_HOPPER_EMPTY = 2,    /* E2 live */
    PROBLEM_BOWL_MISSING = 3,    /* E3 latched */
    PROBLEM_CONNECTION = 4,      /* E4 latched */
    PROBLEM_TIME = 5,            /* E5 live */
    PROBLEM_DAILY_LIMIT = 6,     /* E6 latched, midnight */
    PROBLEM_NOT_EATEN = 7,       /* E7 latched, consumption */
    PROBLEM_TASK_STALL = 9,      /* E9 latched */
    PROBLEM_CODE_MAX = 10
} problem_code_t;

#define PROBLEM_DETAIL_MAX 40u

/* Latched code with a short detail ("stuck", "12 min"); NULL = none. */
void problem_raise(problem_code_t code, const char *detail);
/* Live code follows its condition. */
void problem_set_live(problem_code_t code, bool active);
/* Own clear rule of one latched code (next ok feed, midnight, consumption). */
void problem_clear(problem_code_t code);
/* Clear problem button: every latched code. */
void problem_clear_latched(void);

bool problem_active(void);
bool problem_is_set(problem_code_t code);
/* Most important active code (E1, E2, E3, E7, E6, E9, E4, E5), NONE if none. */
problem_code_t problem_top(void);
/* `["E1","E3"]` */
int problem_format_codes(char *buf, size_t len);
/* `Feed failed (stuck); Bowl missing at feed` */
int problem_format_text(char *buf, size_t len);
/* Bumped on every change (publishers compare). */
uint32_t problem_generation(void);

void problem_test_reset(void);

#endif /* PROBLEM_H */
