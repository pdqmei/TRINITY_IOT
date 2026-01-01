// Main application - FIXED: Buzzer debounce logic
// Buzzer chỉ kêu khi CHUYỂN TRẠNG THÁI, không kêu liên tục

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "app_config.h"
#include "wifi_manager.h"
#include "mqtt_handler.h"
#include "fan.h"
#include "led.h"
#include "buzzer.h"
#include "sht31.h"
#include "mq135.h"
#include "moving_average.h"

static const char *TAG = "MAIN";

// Moving averages
static moving_average_t ma_temp;
static moving_average_t ma_humi;
static moving_average_t ma_air;

// Task handles
static TaskHandle_t sensor_task_handle = NULL;
static TaskHandle_t actuator_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;

// Latest values
static float latest_temp = 0.0f;
static float latest_humi = 0.0f;
static int latest_air_level = 0;
static int latest_mq_raw = 0;
static float latest_mq_ppm = 0.0f;

// 🔑 KEY FIX: Alert state tracking (để tránh kêu liên tục)
static bool alert_temp_critical = false;  // temp > 35°C
static bool alert_temp_high = false;      // temp > 30°C
static bool alert_air_poor = false;       // air >= 3

// Event group
static EventGroupHandle_t sys_event_group;
enum {
    EVT_SENSOR_READY = BIT0,
    EVT_WIFI_CONNECTED = BIT1,
    EVT_MQTT_CONNECTED = BIT2,
};

static bool read_sht31_with_retry(sht31_data_t *out)
{
    for (int i = 0; i < 3; i++) {
        sht31_data_t d = sht31_read();
        if (d.temperature > -40 && d.temperature < 125) {
            *out = d;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

static bool read_mq135_with_retry(int *out_raw)
{
    for (int i = 0; i < 3; i++) {
        uint16_t raw = mq135_read_raw();
        if (raw <= 4095) {
            *out_raw = raw;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

static void sensor_task(void *pvParameters)
{
    (void) pvParameters;

    ma_init(&ma_temp, 10);
    ma_init(&ma_humi, 10);
    ma_init(&ma_air, 10);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        sht31_data_t sdata = {0};
        bool sht_ok = read_sht31_with_retry(&sdata);

        int mq_raw = 0;
        bool mq_ok = read_mq135_with_retry(&mq_raw);

        if (!sht_ok) {
            ESP_LOGE(TAG, "SHT31 read failed, using last value");
        } else {
            ma_add(&ma_temp, sdata.temperature);
            ma_add(&ma_humi, sdata.humidity);
            latest_temp = ma_get(&ma_temp);
            latest_humi = ma_get(&ma_humi);
        }

        if (!mq_ok) {
            ESP_LOGE(TAG, "MQ135 read failed, using last value");
        } else {
            float ppm = mq135_read_ppm();
            
            int level;
            if (ppm >= 0.0f) {
                if (ppm < 400) level = 0;
                else if (ppm < 600) level = 1;
                else if (ppm < 800) level = 2;
                else if (ppm < 1000) level = 3;
                else level = 4;
            } else {
                if (mq_raw <= 819) level = 0;
                else if (mq_raw <= 1638) level = 1;
                else if (mq_raw <= 2457) level = 2;
                else if (mq_raw <= 3276) level = 3;
                else level = 4;
            }
            
            ma_add(&ma_air, (float)level);
            latest_air_level = (int)ma_get(&ma_air);

            latest_mq_raw = mq_raw;
            latest_mq_ppm = (ppm >= 0.0f) ? ppm : 0.0f;

            if (ppm >= 0.0f) {
                ESP_LOGI(TAG, "MQ135: ADC raw=%d, PPM=%.2f, AQ level=%d", mq_raw, ppm, level);
            } else {
                ESP_LOGI(TAG, "MQ135: ADC raw=%d, PPM=N/A, AQ level=%d", mq_raw, level);
            }
        }

        xEventGroupSetBits(sys_event_group, EVT_SENSOR_READY);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

static void actuator_task(void *pvParameters)
{
    (void) pvParameters;

    fan_init();
    led_init();
    buzzer_init();

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(sys_event_group, EVT_SENSOR_READY, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & EVT_SENSOR_READY) {
            ESP_LOGI(TAG, "===== Actuator Control Start =====");
            ESP_LOGI(TAG, "Sensors: T=%.2f°C, H=%.2f%%, AQ=%d", latest_temp, latest_humi, latest_air_level);

            // ========== FAN CONTROL ==========
            uint8_t fan_speed = 0;
            float fan_status = 0.0f;

            if (latest_temp < 25.0f) {
                fan_speed = 0;
                fan_status = 0.0f;
                ESP_LOGI(TAG, "[FAN] OFF (T < 25°C)");
                fan_off();
            } 
            else if (latest_temp < 28.0f) {
                fan_speed = 40;
                fan_status = 0.4f;
                ESP_LOGI(TAG, "[FAN] 40%% (25-28°C)");
                fan_set_speed(fan_speed);
            } 
            else if (latest_temp < 31.0f) {
                fan_speed = 70;
                fan_status = 0.7f;
                ESP_LOGI(TAG, "[FAN] 70%% (28-31°C)");
                fan_set_speed(fan_speed);
            } 
            else {
                fan_speed = 100;
                fan_status = 1.0f;
                ESP_LOGI(TAG, "[FAN] 100%% (>= 31°C)");
                fan_on();
            }

            mqtt_send_data("status/fan", fan_status);

            // ========== LED CONTROL ==========
            const char* aq_desc[] = {"Good", "Fair", "Moderate", "Poor", "Very Poor"};
            ESP_LOGI(TAG, "[LED] Air Quality: %s (level %d)", aq_desc[latest_air_level], latest_air_level);
            
            switch (latest_air_level) {
                case 0: // Green
                    led_set_rgb(0, 1023, 0);
                    break;
                    
                case 1: // Yellow
                    led_set_rgb(1023, 1023, 0);
                    break;
                    
                case 2: // Orange
                    led_set_rgb(1023, 662, 0);
                    break;
                    
                case 3: // Red blink
                    ESP_LOGW(TAG, "[LED] RED BLINK (poor air)");
                    for (int i = 0; i < 2; i++) {
                        led_set_rgb(1023, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        led_set_rgb(0, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                    break;
                    
                case 4: // Purple blink fast
                    ESP_LOGE(TAG, "[LED] PURPLE BLINK FAST (very poor air)");
                    for (int i = 0; i < 3; i++) {
                        led_set_rgb(1023, 0, 1023);
                        vTaskDelay(pdMS_TO_TICKS(200));
                        led_set_rgb(0, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(200));
                    }
                    break;
                    
                default:
                    led_set_rgb(0, 0, 0);
                    break;
            }

            // ========== 🔑 BUZZER CONTROL - DEBOUNCED ALERTS ==========
            // Chỉ kêu khi CHUYỂN từ OK → ALERT, không kêu lại nếu vẫn đang alert
            
            bool needs_alert = false;
            
            // Priority 1: Critical temperature (> 35°C)
            if (latest_temp > 35.0f) {
                if (!alert_temp_critical) {
                    // Chuyển trạng thái: OK → CRITICAL
                    ESP_LOGE(TAG, "[ALERT] CRITICAL TEMP > 35°C - FIRST TIME ALERT!");
                    ESP_LOGE(TAG, "[BUZZER] 5 rapid beeps + LED flash");
                    
                    for (int i = 0; i < 5; i++) {
                        buzzer_on();
                        led_set_rgb(1023, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        buzzer_off();
                        led_set_rgb(0, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    
                    alert_temp_critical = true;
                    needs_alert = true;
                } else {
                    ESP_LOGW(TAG, "[ALERT] Still critical temp (%.2f°C) - no buzzer", latest_temp);
                }
            } else {
                // Reset flag khi nhiệt độ trở lại bình thường
                if (alert_temp_critical) {
                    ESP_LOGI(TAG, "[ALERT] Temp back to safe - resetting critical flag");
                    alert_temp_critical = false;
                }
            }
            
            // Priority 2: Poor air quality (>= 3)
            if (latest_air_level >= 3 && !needs_alert) {
                if (!alert_air_poor) {
                    // Chuyển trạng thái: OK → POOR AIR
                    ESP_LOGW(TAG, "[ALERT] POOR AIR QUALITY (level %d) - FIRST TIME!", latest_air_level);
                    ESP_LOGW(TAG, "[BUZZER] 2 warning beeps");
                    
                    for (int i = 0; i < 2; i++) {
                        buzzer_on();
                        vTaskDelay(pdMS_TO_TICKS(200));
                        buzzer_off();
                        vTaskDelay(pdMS_TO_TICKS(200));
                    }
                    
                    alert_air_poor = true;
                    needs_alert = true;
                } else {
                    ESP_LOGW(TAG, "[ALERT] Still poor air (level %d) - no buzzer", latest_air_level);
                }
            } else {
                // Reset flag khi chất lượng không khí cải thiện
                if (alert_air_poor && latest_air_level < 3) {
                    ESP_LOGI(TAG, "[ALERT] Air quality improved - resetting poor air flag");
                    alert_air_poor = false;
                }
            }
            
            // Priority 3: High temperature (> 30°C) - LED breathing effect only
            if (latest_temp > 30.0f && latest_temp <= 35.0f && !needs_alert) {
                if (!alert_temp_high) {
                    ESP_LOGW(TAG, "[ALERT] HIGH TEMP (%.2f°C) - LED breathing + 1 beep", latest_temp);
                    
                    // Breathing red effect
                    for (int step = 0; step <= 1023; step += 32) {
                        led_set_rgb(step, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(1000 / 32));
                    }
                    for (int step = 1023; step >= 0; step -= 32) {
                        led_set_rgb(step, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(1000 / 32));
                    }
                    
                    // Single beep
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(150));
                    buzzer_off();
                    
                    alert_temp_high = true;
                } else {
                    ESP_LOGW(TAG, "[ALERT] Still high temp (%.2f°C) - no buzzer", latest_temp);
                }
            } else {
                if (alert_temp_high && latest_temp <= 30.0f) {
                    ESP_LOGI(TAG, "[ALERT] Temp normalized - resetting high temp flag");
                    alert_temp_high = false;
                }
            }
            
            // Đảm bảo buzzer tắt sau mỗi chu kỳ
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            
            ESP_LOGI(TAG, "===== Actuator Control End =====");
        }
    }
}

static void mqtt_task(void *pvParameters)
{
    (void) pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"C\"}", latest_temp);
        mqtt_send_data("sensor/temperature", latest_temp);

        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"%%\"}", latest_humi);
        mqtt_send_data("sensor/humidity", latest_humi);

        ESP_LOGI(TAG, "MQ135 last raw=%d PPM=%.2f level=%d", latest_mq_raw, latest_mq_ppm, latest_air_level);
        mqtt_send_data("sensor/air_quality", (float)latest_air_level);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "TRINITY_IOT - Environment Monitor starting");
    esp_log_level_set("MQ135", ESP_LOG_INFO);

    sys_event_group = xEventGroupCreate();

    fan_init();
    led_init();
    buzzer_init();
    sht31_init();
    mq135_init();

    // MQ135 Calibration
    ESP_LOGI(TAG, "Waiting 30 seconds for MQ135 warmup...");
    vTaskDelay(pdMS_TO_TICKS(30000));
    
    if (!mq135_has_calibration()) {
        ESP_LOGI(TAG, "Starting MQ135 calibration...");
        esp_err_t cal_result = mq135_calibrate();
        if (cal_result == ESP_OK) {
            ESP_LOGI(TAG, "✓ MQ135 calibration successful!");
        } else {
            ESP_LOGW(TAG, "✗ MQ135 calibration failed");
        }
    } else {
        ESP_LOGI(TAG, "MQ135 already calibrated");
    }

    wifi_init();
    mqtt_app_start(NULL);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
    xTaskCreate(actuator_task, "actuator_task", 3072, NULL, 6, &actuator_task_handle);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 4, &mqtt_task_handle);

    ESP_LOGI(TAG, "System initialized");
}