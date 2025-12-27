#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

// Định nghĩa cấu trúc sensor_data_t
typedef struct {
    float temperature;      // Nhiệt độ (°C)
    float humidity;         // Độ ẩm (%)
    int co2_level;          // Nồng độ CO2 (ADC raw value)
    float co2_ppm;          // Nồng độ CO2 (ppm)
    bool is_valid;          // Dữ liệu có hợp lệ không
} sensor_data_t;

// Cấu hình MQTT
#define MQTT_BROKER_URI     "mqtt://broker.hivemq.com"
#define MQTT_PORT           1883
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""

#define DEFAULT_ROOM_ID     "bedroom"

// Functions
void mqtt_app_start(const char *room_id);
void mqtt_publish_sensor_data(sensor_data_t data);
void mqtt_publish_actuator_state(const char *actuator, bool state, int level);
void mqtt_set_room(const char *new_room_id);
const char* mqtt_get_room(void);
bool mqtt_is_connected(void);


#endif