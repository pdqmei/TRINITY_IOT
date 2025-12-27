#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "sensor_control.h"

// Cấu hình MQTT
#define MQTT_BROKER_URI     "mqtt://broker.hivemq.com"  // Hoặc broker của bạn
#define MQTT_PORT           1883
#define MQTT_USERNAME       ""  // Nếu có
#define MQTT_PASSWORD       ""  // Nếu có

#define DEFAULT_ROOM_ID     "bedroom"

// Functions
void mqtt_app_start(const char *room_id);  // ← SỬA: thêm tham số
void mqtt_publish_sensor_data(sensor_data_t data);
void mqtt_publish_actuator_state(const char *actuator, bool state, int level);  // ← THÊM MỚI
void mqtt_set_room(const char *new_room_id);  // ← THÊM MỚI
const char* mqtt_get_room(void);  // ← THÊM MỚI
bool mqtt_is_connected(void);


#endif