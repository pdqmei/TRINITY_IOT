#include "mqtt_handler.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "esp_event.h" 
#include "stdint.h"

// Import actuator control functions
#include "fan.h"
#include "led.h"
#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MQTT";

// Buzzer task handle for non-blocking patterns
static TaskHandle_t buzzer_task_handle = NULL;
static volatile int buzzer_pattern_level = 0;
static esp_mqtt_client_handle_t client = NULL;
static bool is_connected = false;

// ===============================================
// 🆕 BIẾN TOÀN CỤC: CHẾ ĐỘ AUTO/MANUAL
// ===============================================
static bool is_auto_mode = true;  // Giá trị mặc định (sẽ đợi sync từ web)
static bool auto_mode_initialized = false;  // ✅ FIX VẤN ĐỀ 1

// Room ID (được set từ main.c)
static char current_room_id[32] = "livingroom";

// Topic subscriptions
static char topic_fan[128];
static char topic_led[128];
static char topic_buzzer[128];
static char topic_auto[64];

static const char *hivemq_ca_cert = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

// ===============================================
// 🆕 HÀM SUBSCRIBE/UNSUBSCRIBE ACTUATOR TOPICS
// ===============================================
static void subscribe_actuator_topics(void)
{
    if (!is_connected || client == NULL) return;

    esp_mqtt_client_subscribe(client, topic_fan, 1);
    esp_mqtt_client_subscribe(client, topic_led, 1);
    esp_mqtt_client_subscribe(client, topic_buzzer, 1);
    
    ESP_LOGI(TAG, "✅ Subscribed to actuator command topics (MANUAL mode)");
}

static void unsubscribe_actuator_topics(void)
{
    if (!is_connected || client == NULL) return;

    esp_mqtt_client_unsubscribe(client, topic_fan);
    esp_mqtt_client_unsubscribe(client, topic_led);
    esp_mqtt_client_unsubscribe(client, topic_buzzer);
    
    ESP_LOGI(TAG, "🔕 Unsubscribed from actuator command topics (AUTO mode)");
}

// ===============================================
// 🆕 HÀM REPORT TRẠNG THÁI THỰC TẾ (FIX VẤN ĐỀ 2)
// ===============================================
static void mqtt_report_actuator_state(const char *device, const char *state, int level, bool success)
{
    if (!is_connected || client == NULL) return;

    char topic[128];
    char payload[128];
    
    // Topic: smarthome/{room}/actuators/{device}/reported
    snprintf(topic, sizeof(topic), "smarthome/%s/actuators/%s/reported", current_room_id, device);
    
    // Payload: {"state":"ON", "level":70, "success":true}
    snprintf(payload, sizeof(payload), 
             "{\"state\":\"%s\",\"level\":%d,\"success\":%s}", 
             state, level, success ? "true" : "false");

    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "📡 Reported %s state: %s", device, payload);
}
// ===============================================
// set level cho fan (0=OFF,1=50%,2=100%)
// 
// ===============================================
static void fan_set_level(int level)
{
    const char* level_names[] = {"OFF", "50", "100"};
    
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    
    ESP_LOGI(TAG, "🌀 FAN Level %d: %s", level, level_names[level]);
    
    switch (level) {
        case 0: // OFF - Good air quality
            fan_off();
            break;
        case 1: // 50% - Moderate air quality
            fan_set_speed(50);
            break;
        case 2: // 100% - Poor air quality
            fan_on(); // Full speed
            break;
        default:
            fan_off();
            break;
    }
}
// ===============================================
// 🆕 BUZZER PATTERN TASK (NON-BLOCKING, LOOPING)
// Chạy pattern trong task riêng, lặp liên tục cho đến khi bị hủy
// ===============================================
static void buzzer_pattern_task(void *pvParameters)
{
    int level = buzzer_pattern_level;
    
    ESP_LOGI(TAG, "🔔 BUZZER Task started - Level %d (looping)", level);
    
    while (1) {
        // Check if task should stop
        if (buzzer_pattern_level == 0) {
            break;
        }
        
        switch (level) {
            case 1:
                // Level 1: Kêu liên tục 5 giây, nghỉ 2 giây
                buzzer_on();
                for (int i = 0; i < 50 && buzzer_pattern_level == level; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                buzzer_off();
                for (int i = 0; i < 20 && buzzer_pattern_level == level; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
                
            case 2:
                // Level 2: Kêu 2s, tắt 0.5s, lặp 5 lần, nghỉ 2s
                for (int cycle = 0; cycle < 5 && buzzer_pattern_level == level; cycle++) {
                    buzzer_on();
                    for (int i = 0; i < 20 && buzzer_pattern_level == level; i++) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    buzzer_off();
                    for (int i = 0; i < 5 && buzzer_pattern_level == level; i++) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
                // Nghỉ 2s trước khi lặp lại
                for (int i = 0; i < 20 && buzzer_pattern_level == level; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
                
            default:
                // Level 3: Kêu 1s, tắt 0.3s, lặp 10 lần, nghỉ 1s
                for (int cycle = 0; cycle < 10 && buzzer_pattern_level == level; cycle++) {
                    buzzer_on();
                    for (int i = 0; i < 10 && buzzer_pattern_level == level; i++) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    buzzer_off();
                    for (int i = 0; i < 3 && buzzer_pattern_level == level; i++) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
                // Nghỉ 1s trước khi lặp lại
                for (int i = 0; i < 10 && buzzer_pattern_level == level; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
        }
        
        // Check again if level changed
        if (buzzer_pattern_level != level) {
            level = buzzer_pattern_level;
            if (level == 0) break;
            ESP_LOGI(TAG, "🔔 BUZZER Level changed to %d", level);
        }
    }
    
    buzzer_off();
    ESP_LOGI(TAG, "🔕 BUZZER Task stopped");
    buzzer_task_handle = NULL;
    vTaskDelete(NULL);
}

// Khởi chạy buzzer pattern trong task riêng (non-blocking, looping)
static void buzzer_start_pattern(int level)
{
    // Nếu level giống cũ và task đang chạy, không làm gì
    if (level == buzzer_pattern_level && buzzer_task_handle != NULL) {
        ESP_LOGI(TAG, "🔔 BUZZER already running at level %d", level);
        return;
    }
    
    // Update level (task sẽ tự detect thay đổi)
    buzzer_pattern_level = level;
    
    if (level <= 0) {
        // Level 0 = stop
        buzzer_off();
        ESP_LOGI(TAG, "🔕 BUZZER OFF");
        // Task sẽ tự thoát khi thấy level = 0
        return;
    }
    
    // Nếu chưa có task, tạo mới
    if (buzzer_task_handle == NULL) {
        xTaskCreate(buzzer_pattern_task, "buzzer_pattern", 2048, 
                    NULL, 5, &buzzer_task_handle);
        ESP_LOGI(TAG, "🔔 BUZZER started - Level %d (continuous)", level);
    } else {
        // Task đang chạy, chỉ cần thay đổi level
        ESP_LOGI(TAG, "🔔 BUZZER level changed to %d", level);
    }
}

// ===============================================
// 🆕 HÀM SET LED THEO LEVEL (NON-BLOCKING)
// LED Colors by Air Quality Level:
// Level 0: Green      (0, 1023, 0)     - Good
// Level 1: Cyan       (0, 1023, 1023)  - Fair
// Level 2: Yellow     (1023, 1023, 0)  - Moderate
// Level 3: Red        (1023, 0, 0)     - Poor
// Level 4: Purple     (1023, 0, 1023)  - Very Poor
// ===============================================
static void led_set_level(int level)
{
    const char* level_names[] = {"Green (Good)", "Cyan (Fair)", "Yellow (Moderate)", 
                                  "Red (Poor)", "Purple (Very Poor)"};
    
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    
    ESP_LOGI(TAG, "💡 LED Level %d: %s", level, level_names[level]);
    
    switch (level) {
        case 0: // Green - Good
            led_set_rgb(0, 1023, 0);
            break;
        case 1: // Cyan - Fair
            led_set_rgb(0, 1023, 1023);
            break;
        case 2: // Yellow - Moderate
            led_set_rgb(1023, 1023, 0);
            break;
        case 3: // Red - Poor
            led_set_rgb(1023, 0, 0);
            break;
        case 4: // Purple - Very Poor
            led_set_rgb(1023, 0, 1023);
            break;
        default:
            led_set_rgb(0, 0, 0);
            break;
    }
}

// ===============================================
// 🆕 HÀM XỬ LÝ LỆNH ĐIỀU KHIỂN TỪ WEB
// ===============================================
static void handle_actuator_command(const char *topic, const char *payload)
{
    // ✅ FIX VẤN ĐỀ 1: Kiểm tra initialized
    if (!auto_mode_initialized) {
        ESP_LOGW(TAG, "⚠️ Auto mode not initialized, waiting for smarthome/auto");
        return;
    }

    // Parse JSON: {"state":"ON", "level":70}
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", payload);
        return;
    }

    const char *state = NULL;
    int level = 0;
    bool success = true;

    cJSON *state_item = cJSON_GetObjectItem(root, "state");
    if (state_item && cJSON_IsString(state_item)) {
        state = state_item->valuestring;
    }

    cJSON *level_item = cJSON_GetObjectItem(root, "level");
    if (level_item && cJSON_IsNumber(level_item)) {
        level = level_item->valueint;
    }

    ESP_LOGI(TAG, "📥 Command: %s → state=%s, level=%d", topic, state ? state : "NULL", level);

    // Xác định thiết bị từ topic
    const char *device_name = NULL;
    
    // ========== FAN CONTROL - FIXED VERSION ==========
    
    if (strstr(topic, "/fan") != NULL) {
        device_name = "fan";
        
        if (state && strcmp(state, "ON") == 0) {
            // ✅ FIX: Sử dụng level trực tiếp từ web (0, 1, 2)
            int fan_level = level;
            
            // Đảm bảo level trong khoảng hợp lệ
            if (fan_level < 0) fan_level = 0;
            if (fan_level > 2) fan_level = 2;
            
            fan_set_level(fan_level);
            level = fan_level; // Report level đã sử dụng
            
            const char* level_desc[] = {"OFF", "50%", "100%"};
            ESP_LOGI(TAG, "✅ FAN set to level %d (%s)", fan_level, level_desc[fan_level]);
        } else {
            fan_set_level(0); // OFF
            level = 0;
            ESP_LOGI(TAG, "✅ FAN OFF");
        }
    }
    // ========== LED CONTROL ==========
    // Level: 0=Green, 1=Cyan, 2=Yellow, 3=Red, 4=Purple (match AUTO mode)
    else if (strstr(topic, "/led") != NULL) {
        device_name = "led";
        
        if (state && strcmp(state, "ON") == 0) {
            led_set_level(level);
        } else {
            led_set_rgb(0, 0, 0);
            level = 0;
            ESP_LOGI(TAG, "✅ LED OFF");
        }
    }
    // ========== BUZZER CONTROL ==========
    // Level: 0=OFF, 1-3=pattern (match AUTO mode, looping until stopped)
    else if (strstr(topic, "/buzzer") != NULL) {
        device_name = "buzzer";
        
        if (state && strcmp(state, "ON") == 0) {
            buzzer_start_pattern(level);  // Non-blocking, looping!
        } else {
            buzzer_start_pattern(0);  // Stop
            level = 0;
        }
    }

    // ✅ FIX VẤN ĐỀ 2: Report trạng thái thực tế phần cứng
    if (device_name != NULL) {
        mqtt_report_actuator_state(device_name, state ? state : "OFF", level, success);
    }

    cJSON_Delete(root);
}

// ===============================================
// 🆕 MQTT EVENT HANDLER - ĐẦY ĐỦ CHỨC NĂNG
// ===============================================
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT connected");
            is_connected = true;

            // Tạo topics
            snprintf(topic_fan, sizeof(topic_fan), "smarthome/%s/actuators/fan", current_room_id);
            snprintf(topic_led, sizeof(topic_led), "smarthome/%s/actuators/led", current_room_id);
            snprintf(topic_buzzer, sizeof(topic_buzzer), "smarthome/%s/actuators/buzzer", current_room_id);
            snprintf(topic_auto, sizeof(topic_auto), "smarthome/auto");

            // ✅ LUÔN subscribe topic auto mode
            esp_mqtt_client_subscribe(client, topic_auto, 1);
            ESP_LOGI(TAG, "📩 Subscribed to: %s", topic_auto);

            // ✅ FIX VẤN ĐỀ 3: Chỉ subscribe actuator nếu MANUAL mode
            if (!is_auto_mode && auto_mode_initialized) {
                subscribe_actuator_topics();
            } else {
                ESP_LOGI(TAG, "ℹ️ Waiting for auto mode initialization...");
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "❌ MQTT disconnected");
            is_connected = false;
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "📩 MQTT RX: %.*s → %.*s", 
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            // ✅ XỬ LÝ CHUYỂN ĐỔI AUTO/MANUAL
            if (strstr(event->topic, "smarthome/auto") != NULL) {
                cJSON *root = cJSON_Parse(event->data);
                if (root) {
                    cJSON *state = cJSON_GetObjectItem(root, "state");
                    if (state && cJSON_IsString(state)) {
                        bool new_mode = (strcmp(state->valuestring, "ON") == 0);
                        
                        // ✅ FIX VẤN ĐỀ 1: Đánh dấu đã initialized
                        if (!auto_mode_initialized) {
                            auto_mode_initialized = true;
                            ESP_LOGI(TAG, "✅ Auto mode initialized from web");
                        }
                        
                        // ✅ FIX VẤN ĐỀ 3: Subscribe/Unsubscribe theo mode
                        if (new_mode != is_auto_mode) {
                            is_auto_mode = new_mode;
                            
                            if (is_auto_mode) {
                                ESP_LOGI(TAG, "🤖 Switched to AUTO mode");
                                unsubscribe_actuator_topics();
                                
                                // ✅ Reset buzzer pattern task từ MANUAL mode
                                buzzer_start_pattern(0);
                                
                                // ✅ TRIGGER ACTUATOR UPDATE NGAY LẬP TỨC
                                // Để fan/LED/buzzer cập nhật theo sensor hiện tại
                                trigger_actuator_update();
                                ESP_LOGI(TAG, "🔄 Actuators will update to current sensor values");
                            } else {
                                ESP_LOGI(TAG, "👤 Switched to MANUAL mode");
                                subscribe_actuator_topics();
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            // ✅ CHỈ XỬ LÝ LỆNH ĐIỀU KHIỂN KHI Ở MANUAL MODE
            else if (!is_auto_mode && strstr(event->topic, "/actuators/") != NULL && 
                     strstr(event->topic, "/reported") == NULL) {  // Không xử lý topic reported
                
                // Copy topic và data vì event->data không kết thúc bằng \0
                char topic_str[256] = {0};
                char data_str[512] = {0};
                
                int topic_len = (event->topic_len < 255) ? event->topic_len : 255;
                int data_len = (event->data_len < 511) ? event->data_len : 511;
                
                strncpy(topic_str, event->topic, topic_len);
                strncpy(data_str, event->data, data_len);
                
                handle_actuator_command(topic_str, data_str);
            }
            else if (is_auto_mode && strstr(event->topic, "/actuators/") != NULL) {
                ESP_LOGW(TAG, "⚠️ Ignored actuator command (AUTO mode active)");
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT error");
            break;

        default:
            break;
    }
}

// ===============================================
// HÀM PUBLISH SENSOR DATA
// ===============================================
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
        ESP_LOGI(TAG, "📤 Published sensor: %s → %s", topic, payload);
    } else {
        ESP_LOGE(TAG, "❌ Failed to publish to %s", topic);
    }
}

// ===============================================
// HÀM PUBLISH ACTUATOR STATUS (CHỈ TRONG AUTO MODE)
// ===============================================
void mqtt_publish_actuator(const char *topic, const char *state, int level)
{
    if (!is_connected || client == NULL) return;

    // ✅ FIX VẤN ĐỀ 1: Kiểm tra initialized
    if (!auto_mode_initialized) {
        ESP_LOGD(TAG, "⏭️ Skip publishing actuator (not initialized)");
        return;
    }

    // ✅ CHỈ PUBLISH KHI Ở AUTO MODE
    if (!is_auto_mode) {
        ESP_LOGD(TAG, "⏭️ Skip publishing actuator (MANUAL mode)");
        return;
    }

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"state\":\"%s\",\"level\":%d}", state, level);

    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "📤 Published actuator: %s → %s", topic, payload);
}

// ===============================================
// KHỞI TẠO MQTT
// ===============================================
void mqtt_app_start(const char *custom_room_id)
{
    if (custom_room_id != NULL) {
        strncpy(current_room_id, custom_room_id, sizeof(current_room_id) - 1);
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://19059388a61f4c8286066fda62e74315.s1.eu.hivemq.cloud:8883",
        .credentials.username = "trinity",
        .credentials.authentication.password = "Hung123456789",
        .broker.verification.certificate = hivemq_ca_cert,
        .broker.verification.use_global_ca_store = false,
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    ESP_LOGI(TAG, "🚀 MQTT started for room: %s", current_room_id);
    ESP_LOGI(TAG, "⏳ Waiting for auto mode initialization from web...");
}

// ===============================================
// 🆕 HÀM GETTER CHO AUTO MODE & CONNECTION STATUS
// ===============================================
bool mqtt_is_auto_mode(void)
{
    return is_auto_mode;
}

bool mqtt_is_connected(void)
{
    return is_connected;
}
bool mqtt_is_auto_mode_initialized(void)
{
    return is_auto_mode && auto_mode_initialized;
} 