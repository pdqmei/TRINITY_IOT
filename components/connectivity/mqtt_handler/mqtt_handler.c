#include "mqtt_handler.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
static bool is_connected = false;

// Minimal publish helper used by main
void mqtt_send_data(const char* topic, float value)
{
    if (!is_connected || client == NULL) {
        ESP_LOGW(TAG, "MQTT not connected, cannot send data");
        return;
    }

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"value\":%.2f}", value);

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
    } else {
        ESP_LOGE(TAG, "Failed to publish to %s", topic);
    }
}

void mqtt_app_start(const char *custom_room_id)
{
    // Simple default start - real configuration via menuconfig / sdkconfig
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com",
        .broker.address.port = 1883,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT started (hivemq public broker)");
    is_connected = true; // optimistic; proper event handling can update this
}

bool mqtt_is_connected(void)
{
    return is_connected;
}
