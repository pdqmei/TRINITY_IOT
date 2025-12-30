#include "mqtt_handler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include <stdio.h>

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
static bool is_connected = false;

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
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com",
        .broker.address.port = 1883,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT started (hivemq public broker)");
    is_connected = true;
}

bool mqtt_is_connected(void)
{
    return is_connected;
}
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

static char room_id[32] = "bedroom";  // Default room

// ==================== THÊM MỚI: Callback điều khiển từ sensor_control.c ====================
// Cần define trong sensor_control.c
extern void control_fan(bool state, int level);
extern void control_led(bool state, int level);
extern void control_whistle(bool state, int level);

// ==================== GIỮ NGUYÊN: build_topic ====================
static void build_topic(char *buffer, const char *category, 
                       const char *device, const char *action) {
    if (device && action) {
        sprintf(buffer, "smarthome/%s/%s/%s/%s", 
                room_id, category, device, action);
    } else if (device) {
        sprintf(buffer, "smarthome/%s/%s/%s", 
                room_id, category, device);
    } else {
        sprintf(buffer, "smarthome/%s/%s", room_id, category);
    }
}

// ==================== THÊM MỚI: Xử lý lệnh điều khiển ====================
static void handle_control_command(const char *topic, int topic_len,
                                   const char *data, int data_len) {
    char topic_str[128] = {0};
    char data_str[256] = {0};
    
    // Copy để xử lý an toàn
    strncpy(topic_str, topic, topic_len < 127 ? topic_len : 127);
    strncpy(data_str, data, data_len < 255 ? data_len : 255);
    
    ESP_LOGI(TAG, "Control command on %s: %s", topic_str, data_str);
    
    // Parse JSON: {"state":"on","level":2}
    cJSON *json = cJSON_Parse(data_str);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }
    
    cJSON *state_json = cJSON_GetObjectItem(json, "state");
    cJSON *level_json = cJSON_GetObjectItem(json, "level");
    
    if (!state_json || !level_json) {
        ESP_LOGE(TAG, "Missing 'state' or 'level' in JSON");
        cJSON_Delete(json);
        return;
    }
    
    bool state = (strcmp(state_json->valuestring, "on") == 0);
    int level = level_json->valueint;
    
    // Xác định thiết bị và điều khiển
    if (strstr(topic_str, "/fan/set")) {
        ESP_LOGI(TAG, "Controlling FAN: state=%s, level=%d", 
                 state ? "ON" : "OFF", level);
        control_fan(state, level);
        mqtt_publish_actuator_state("fan", state, level);
        
    } else if (strstr(topic_str, "/led/set")) {
        ESP_LOGI(TAG, "Controlling LED: state=%s, level=%d", 
                 state ? "ON" : "OFF", level);
        control_led(state, level);
        mqtt_publish_actuator_state("led", state, level);
        
    } else if (strstr(topic_str, "/whistle/set")) {
        ESP_LOGI(TAG, "Controlling WHISTLE: state=%s, level=%d", 
                 state ? "ON" : "OFF", level);
        control_whistle(state, level);
        mqtt_publish_actuator_state("whistle", state, level);
    } else {
        ESP_LOGW(TAG, "Unknown device in topic: %s", topic_str);
    }
    
    cJSON_Delete(json);
}

// ==================== SỬA: MQTT Event Handler ====================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    char topic_buffer[128];
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected! Room: %s", room_id);
            is_connected = true;
            
            // Subscribe các topic điều khiển
            build_topic(topic_buffer, "actuators", "fan", "set");
            esp_mqtt_client_subscribe(client, topic_buffer, 1);
            ESP_LOGI(TAG, "Subscribed: %s", topic_buffer);
            
            build_topic(topic_buffer, "actuators", "led", "set");
            esp_mqtt_client_subscribe(client, topic_buffer, 1);
            ESP_LOGI(TAG, "Subscribed: %s", topic_buffer);
            
            build_topic(topic_buffer, "actuators", "whistle", "set");
            esp_mqtt_client_subscribe(client, topic_buffer, 1);
            ESP_LOGI(TAG, "Subscribed: %s", topic_buffer);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            is_connected = false;
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Message published");
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received command:");
            ESP_LOGI(TAG, "  Topic: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "  Data: %.*s", event->data_len, event->data);
            
            // Xử lý lệnh điều khiển
            handle_control_command(event->topic, event->topic_len,
                                  event->data, event->data_len);
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error");
            break;
            
        default:
            break;
    }
}

// ==================== SỬA: mqtt_app_start với room_id ====================
void mqtt_app_start(const char *custom_room_id)
{
    // Cập nhật room_id nếu được cung cấp
    if (custom_room_id != NULL) {
        strncpy(room_id, custom_room_id, sizeof(room_id) - 1);
        room_id[sizeof(room_id) - 1] = '\0';  // Đảm bảo null-terminated
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    ESP_LOGI(TAG, "MQTT started for room: %s", room_id);
}

// ==================== SỬA: Publish sensors với topic động ====================
void mqtt_publish_sensor_data(sensor_data_t data)
{
    if (!is_connected) {
        ESP_LOGW(TAG, "MQTT not connected, skipping publish");
        return;
    }
    
    char topic[128];
    char payload[256];
    
    // Build topic: smarthome/bedroom/sensors
    build_topic(topic, "sensors", NULL, NULL);
    
    // Tạo JSON payload
    snprintf(payload, sizeof(payload),
             "{\"temp\":%d,\"humi\":%d,\"co2\":%d,\"timestamp\":%lld}",
             (int)data.temperature,
             (int)data.humidity,
             (int)data.co2_ppm,
             (long long)(esp_timer_get_time() / 1000000));
    
    // Publish với QoS=0 (fire and forget)
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);
    
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
    } else {
        ESP_LOGE(TAG, "Failed to publish sensor data");
    }
}

// ==================== THÊM MỚI: Publish trạng thái actuator ====================
void mqtt_publish_actuator_state(const char *actuator, bool state, int level)
{
    if (!is_connected) {
        ESP_LOGW(TAG, "MQTT not connected");
        return;
    }
    
    char topic[128];
    char payload[128];
    
    // Build topic: smarthome/bedroom/actuators/fan/state
    build_topic(topic, "actuators", actuator, "state");
    
    snprintf(payload, sizeof(payload),
             "{\"state\":\"%s\",\"level\":%d,\"timestamp\":%lld}",
             state ? "on" : "off",
             level,
             (long long)(esp_timer_get_time() / 1000000));
    
    // Publish với QoS=1 và Retained=1 để lưu trạng thái cuối
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 1);
    
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "State published to %s: %s", topic, payload);
    } else {
        ESP_LOGE(TAG, "Failed to publish state");
    }
}

// ==================== THÊM MỚI: Đổi phòng động ====================
void mqtt_set_room(const char *new_room_id)
{
    if (new_room_id == NULL) {
        ESP_LOGW(TAG, "NULL room_id provided");
        return;
    }
    
    strncpy(room_id, new_room_id, sizeof(room_id) - 1);
    room_id[sizeof(room_id) - 1] = '\0';
    
    ESP_LOGI(TAG, "Room changed to: %s", room_id);
    
    // Reconnect để subscribe topics mới
    if (is_connected && client != NULL) {
        esp_mqtt_client_reconnect(client);
    }
}

// ==================== THÊM MỚI: Lấy room_id hiện tại ====================
const char* mqtt_get_room(void)
{
    return room_id;
}

// ==================== GIỮ NGUYÊN ====================
bool mqtt_is_connected(void)
{
    return is_connected;
}