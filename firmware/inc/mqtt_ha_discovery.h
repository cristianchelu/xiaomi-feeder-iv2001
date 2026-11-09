/*
 * Home Assistant MQTT discovery — spec/30-processes/mqtt-protocol.md
 */

#ifndef MQTT_HA_DISCOVERY_H
#define MQTT_HA_DISCOVERY_H

#include <stdbool.h>
#include <stddef.h>

#include "mqtt_cred.h"

#define MQTT_HA_MANUFACTURER  "Xiaomi"
#define MQTT_HA_MODEL         "Smart Pet Food Feeder 2"

int mqtt_ha_format_dispense_button_config(char *buf,
                                          size_t len,
                                          const char *device_id);

int mqtt_ha_format_bowl_error_config(char *buf,
                                     size_t len,
                                     const char *device_id);

int mqtt_ha_format_bowl_weight_config(char *buf,
                                      size_t len,
                                      const char *device_id);

int mqtt_ha_format_battery_config(char *buf,
                                  size_t len,
                                  const char *device_id);

int mqtt_ha_format_battery_voltage_config(char *buf,
                                          size_t len,
                                          const char *device_id);

int mqtt_ha_format_mains_config(char *buf,
                                size_t len,
                                const char *device_id);

int mqtt_ha_format_hopper_level_config(char *buf,
                                       size_t len,
                                       const char *device_id);

int mqtt_ha_format_device_timezone_config(char *buf,
                                          size_t len,
                                          const char *device_id);

int mqtt_ha_format_feeding_schedule_config(char *buf,
                                           size_t len,
                                           const char *device_id);

int mqtt_ha_format_dispense_completed_config(char *buf,
                                             size_t len,
                                             const char *device_id);

int mqtt_ha_format_weight_compensation_config(char *buf,
                                              size_t len,
                                              const char *device_id);

int mqtt_ha_format_overfill_protection_config(char *buf,
                                              size_t len,
                                              const char *device_id);

int mqtt_ha_format_overfill_threshold_g_config(char *buf,
                                               size_t len,
                                               const char *device_id);

int mqtt_ha_format_child_lock_config(char *buf,
                                     size_t len,
                                     const char *device_id);

int mqtt_ha_format_reboot_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_calibrate_zero_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_portion_size_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_portion_cal_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_battery_wifi_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_ota_base_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_ota_install_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_eaten_reset_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_calibrate_span_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_display_mode_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_display_brightness_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_display_battery_off_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_eaten_today_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_dispense_amount_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_display_mains_off_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_dispense_progress_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_wifi_rssi_config(char *buf, size_t len, const char *device_id);
/* Device page link shown in HA device info (configuration_url). */
void mqtt_ha_discovery_set_ip(const char *ip);
int mqtt_ha_format_hopper_ir_config(char *buf, size_t len, const char *device_id);

int mqtt_ha_format_ota_state_config(char *buf, size_t len, const char *device_id);
int mqtt_ha_format_firmware_bank_config(char *buf, size_t len, const char *device_id);

/* Number of entities in the discovery table. */
unsigned mqtt_ha_discovery_entity_count(void);

/* Outbox occupancy above which the discovery pump waits (spec § Discovery pacing). */
#define MQTT_HA_DISCOVERY_MAX_PENDING  12u

/* Arm paced discovery from the first entity; nothing is enqueued yet. */
void mqtt_ha_discovery_schedule(const char *device_id);

/* Enqueue the next entity when the outbox has room. Returns true if one was enqueued. */
bool mqtt_ha_discovery_pump(void);

/* Entities not yet enqueued (0 when disarmed or done). */
unsigned mqtt_ha_discovery_remaining(void);

/* Stop pending discovery (session ended). */
void mqtt_ha_discovery_disarm(void);

void mqtt_ha_discovery_test_reset(void);

#endif /* MQTT_HA_DISCOVERY_H */
