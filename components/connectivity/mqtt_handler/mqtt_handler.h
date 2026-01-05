#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_event.h"

void mqtt_app_start(const char *custom_room_id);
void mqtt_send_data(const char* topic, float value);
void mqtt_publish_actuator(const char *topic, const char *state, int level);

bool mqtt_is_auto_mode(void);
bool mqtt_is_connected(void);
bool mqtt_is_auto_mode_initialized(void);

// Khai báo extern - hàm được định nghĩa trong main.c
extern void trigger_actuator_update(void);

#endif
