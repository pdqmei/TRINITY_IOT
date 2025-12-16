#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device_control.h"
#include "sensor_control.h"
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "firebase_handler.h"

static const char *TAG = "MAIN";

// Ngưỡng cảnh báo
#define TEMP_THRESHOLD          30.0    // °C
#define HUMIDITY_THRESHOLD      80.0    // %
#define CO2_THRESHOLD           1000.0  // ppm

void control_logic_task(void *pvParameters)
{
    int count = 0;
    
    while(1) {
        sensor_data_t data = read_all_sensors();
        
        if (data.is_valid) {
            count++;
            
            ESP_LOGI(TAG, "=================================");
            ESP_LOGI(TAG, "Reading #%d", count);
            ESP_LOGI(TAG, "  Temperature : %.1f C", data.temperature);
            ESP_LOGI(TAG, "  Humidity    : %.1f %%", data.humidity);
            ESP_LOGI(TAG, "  CO2 Level   : %.1f ppm", data.co2_ppm);
            ESP_LOGI(TAG, "=================================");
            
            // Publish lên MQTT
            if (mqtt_is_connected()) {
                mqtt_publish_sensor_data(data);
            }
            // Publish lên Firebase <--- THÊM KHỐI NÀY
            if (wifi_is_connected()) {
                // Sử dụng room_id hiện tại từ MQTT handler
                firebase_send_sensor_data(data, mqtt_get_room()); 
            }
            // Logic điều khiển
            if (data.temperature > 35.0) {
                fan_speed(100);
            } else if (data.temperature > 30.0) {
                fan_speed(60);
            } else {
                fan_off();
            }
            
            if (data.humidity > HUMIDITY_THRESHOLD) {
                led_on();
            } else {
                led_off();
            }
            
            if (data.co2_ppm > CO2_THRESHOLD) {
                buzzer_beep(3);
            }
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// ... (các hàm khác)

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   SMART ENVIRONMENTAL MONITORING");
    ESP_LOGI(TAG, "   With WiFi + MQTT + Firebase");
    ESP_LOGI(TAG, "========================================");
    
    // 1. Khởi tạo và kết nối WiFi
    ESP_LOGI(TAG, "Connecting to WiFi...");
    
    wifi_init_sta(); 
    
    // 2. Khởi tạo phần cứng và sensors
    device_init();
    sensor_init();
    
    // 3. Khởi động MQTT (Cần tham số room_id)
    ESP_LOGI(TAG, "Starting MQTT...");
    const char *my_room = "livingroom";
    mqtt_app_start(my_room);
    
    // 4. Khởi động Firebase 
    ESP_LOGI(TAG, "Starting Firebase...");
    firebase_init(); 
    
    // 5. Tạo task điều khiển
    xTaskCreate(control_logic_task, "control_logic", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System started!");
}