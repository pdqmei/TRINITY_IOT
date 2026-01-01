#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#define MQTT_BROKER_URI "mqtts://19059388a61f4c8286066fda62e74315.s1.eu.hivemq.cloud"

void mqtt_app_start(const char *custom_room_id);
void mqtt_send_data(const char* topic, float value);
bool mqtt_is_connected(void);

#endif
