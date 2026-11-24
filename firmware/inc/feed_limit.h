/*
 * Daily limit and not-eaten alert — spec/30-processes/health-monitoring.md
 */

#ifndef FEED_LIMIT_H
#define FEED_LIMIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port_err.h"
#include "weight_units.h"

#define FEED_LIMIT_MAX_G          2000u
#define FEED_LIMIT_MIN_G            10u
#define FEED_NOT_EATEN_MAX_H        24u
#define FEED_NOT_EATEN_MIN_DG       50   /* [tune] 5 g counts as a meal / as eating */

uint16_t feed_limit_daily_max_g(void);       /* 0 = off */
uint8_t feed_limit_not_eaten_h(void);        /* 0 = off */
bool feed_limit_set_daily_max_g(uint16_t g);
bool feed_limit_set_not_eaten_h(uint8_t h);

/* Grams still allowed today; false when no limit is set. */
bool feed_limit_remaining_g(uint32_t *remaining_g);

/* Not-eaten alert: a job delivered `delivered_dg` (measured). */
void feed_not_eaten_on_feed(weight_dg_t delivered_dg, uint32_t now_ms);
/* Called every display tick with the eaten-today total. */
void feed_not_eaten_poll(weight_dg_t eaten_today_dg, uint32_t now_ms);

/* MQTT: cmd/feed_limit, retained feed/limit. */
void mqtt_feed_limit_set_device_id(const char *device_id);
void mqtt_feed_limit_connect_snapshot(void);
void mqtt_feed_limit_sync(void);
int mqtt_feed_limit_format(char *buf, size_t len);
port_err_t mqtt_feed_limit_handle(const void *payload, size_t len);

void feed_limit_test_reset(void);

#endif /* FEED_LIMIT_H */
