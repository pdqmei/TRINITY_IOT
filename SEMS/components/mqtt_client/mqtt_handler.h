#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "sensor_types.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

// ========== CHỈNH SỬA: Cấu hình HiveMQ Cluster ==========
#define MQTT_BROKER_URI "mqtts://19059388a61f4c8286066fda62e74315.s1.eu.hivemq.cloud"
#define MQTT_PORT           8883  


#define MQTT_USERNAME       "trinity"  // Điền username HiveMQ
#define MQTT_PASSWORD       "Hung123456789"  // Điền password HiveMQ

#define DEFAULT_ROOM_ID     "bedroom"

// Functions
void mqtt_app_start(const char *room_id);
void mqtt_publish_sensor_data(sensor_data_t data);
void mqtt_publish_actuator_state(const char *actuator, bool state, int level);
void mqtt_set_room(const char *new_room_id);
const char* mqtt_get_room(void);
bool mqtt_is_connected(void);

#endif