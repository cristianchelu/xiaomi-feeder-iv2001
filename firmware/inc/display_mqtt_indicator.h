/*
 * MQTT status lightbar policy — spec/30-processes/display-presentation.md § MQTT indicator
 */

#ifndef DISPLAY_MQTT_INDICATOR_H
#define DISPLAY_MQTT_INDICATOR_H

#include <stdbool.h>

void display_mqtt_indicator_connecting(void);
void display_mqtt_indicator_connected(void);
void display_mqtt_indicator_error(void);
void display_mqtt_indicator_off(void);
/* Device health: with the broker connected the bar is green when healthy and
 * steady orange while a problem is active (bowl error, hopper low/empty, low battery). */
void display_mqtt_indicator_set_problem(bool problem);
bool display_mqtt_indicator_problem(void);

#endif /* DISPLAY_MQTT_INDICATOR_H */
