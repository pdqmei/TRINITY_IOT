#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

void mqtt_app_start(const char *custom_room_id);
void mqtt_send_data(const char* topic, float value);
bool mqtt_is_connected(void);

#endif
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

void mqtt_app_start(void); // Hàm khởi động MQTT
void mqtt_send_data(const char* topic, float value); // Gửi dữ liệu cảm biến

#endif