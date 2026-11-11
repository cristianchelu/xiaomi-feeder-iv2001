/*
 * Wi-Fi signal strength — spec/30-processes/mqtt-protocol.md § Wi-Fi signal
 */

#ifndef MQTT_WIFI_RSSI_H
#define MQTT_WIFI_RSSI_H

#include <stdbool.h>
#include <stdint.h>

#define MQTT_WIFI_RSSI_PERIOD_MS  60000u    /* [tune] sample */
#define MQTT_WIFI_RSSI_PUBLISH_MS 600000u   /* [tune] publish at least */
#define MQTT_WIFI_RSSI_DELTA_DB   5         /* [tune] or on this change */

void mqtt_wifi_rssi_set_device_id(const char *device_id);
/* Sample and publish (retained plain dBm) at most once per period, or on connect. */
void mqtt_wifi_rssi_poll(uint32_t now_ms, bool force);
/* Last sampled value; false before the first sample. */
bool mqtt_wifi_rssi_last(int8_t *dbm);

void mqtt_wifi_rssi_test_reset(void);

#endif /* MQTT_WIFI_RSSI_H */
