#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "sensor_types.h"

// ========== CHỈNH SỬA: Cấu hình HiveMQ Cluster ==========
#define MQTT_BROKER_URI     "mqtt://9dbba28f07a547fa84c9e4b387ec1ff3.s1.eu.hivemq.cloud"
#define MQTT_PORT           8883  


#define MQTT_USERNAME       "trinity"  // Điền username HiveMQ
#define MQTT_PASSWORD       "Trinity3"  // Điền password HiveMQ

#define DEFAULT_ROOM_ID     "bedroom"

// Functions
void mqtt_app_start(const char *room_id);
void mqtt_publish_sensor_data(sensor_data_t data);
void mqtt_publish_actuator_state(const char *actuator, bool state, int level);
void mqtt_set_room(const char *new_room_id);
const char* mqtt_get_room(void);
bool mqtt_is_connected(void);

#endif