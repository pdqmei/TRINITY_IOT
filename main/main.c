// Main application - 4-Level Buzzer System + LCD Display
// Buzzer: Level 0 (OFF) → Level 1 (5s) → Level 2 (2s x5) → Level 3 (1s x10)

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
#include "lcd1602.h"

static const char *TAG = "MAIN";

// Moving averages
static moving_average_t ma_temp;
static moving_average_t ma_humi;
static moving_average_t ma_air;

// Task handles
static TaskHandle_t sensor_task_handle = NULL;
static TaskHandle_t actuator_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;
static TaskHandle_t lcd_task_handle = NULL;

// Latest values
static float latest_temp = 0.0f;
static float latest_humi = 0.0f;
static int latest_air_level = 0;
static int latest_mq_raw = 0;
static float latest_mq_ppm = 0.0f;

// ========== 4-LEVEL BUZZER SYSTEM ==========
typedef enum {
    BUZZER_OFF = 0,           // Tắt
    BUZZER_WARN_5S = 1,       // Cảnh báo: kêu liên tục 5 giây
    BUZZER_ALERT_2S_5X = 2,   // Nguy hiểm: kêu 2s, lặp 5 lần
    BUZZER_CRITICAL_1S_10X = 3 // Nghiêm trọng: kêu 1s, lặp 10 lần liên tục
} buzzer_level_t;

static buzzer_level_t current_buzzer_level = BUZZER_OFF;
static uint32_t buzzer_cycle_count = 0;

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

// ========== BUZZER PATTERN FUNCTIONS ==========
static void buzzer_play_pattern(buzzer_level_t level)
{
    switch (level) {
        case BUZZER_OFF:
            buzzer_off();
            ESP_LOGI(TAG, "[BUZZER] Level 0: OFF");
            break;
            
        case BUZZER_WARN_5S:
            // Level 1: Kêu liên tục 5 giây
            ESP_LOGW(TAG, "[BUZZER] Level 1: WARNING - 5 seconds continuous beep");
            buzzer_on();
            vTaskDelay(pdMS_TO_TICKS(5000));
            buzzer_off();
            break;
            
        case BUZZER_ALERT_2S_5X:
            // Level 2: Kêu 2s, tắt 0.5s, lặp 5 lần
            ESP_LOGW(TAG, "[BUZZER] Level 2: ALERT - 2s beep x 5 cycles");
            for (int i = 0; i < 5; i++) {
                ESP_LOGW(TAG, "  Alert cycle %d/5", i+1);
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(2000));
                buzzer_off();
                if (i < 4) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }
            break;
            
        case BUZZER_CRITICAL_1S_10X:
            // Level 3: Kêu 1s, tắt 0.3s, lặp 10 lần
            ESP_LOGE(TAG, "[BUZZER] Level 3: CRITICAL - 1s beep x 10 cycles CONTINUOUS!");
            for (int i = 0; i < 10; i++) {
                ESP_LOGE(TAG, "  CRITICAL cycle %d/10", i+1);
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(1000));
                buzzer_off();
                if (i < 9) {
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
            }
            break;
            
        default:
            buzzer_off();
            break;
    }
    
    buzzer_off();  // Đảm bảo tắt
}

static buzzer_level_t calculate_buzzer_level(void)
{
    // Priority 1: CRITICAL TEMPERATURE (> 35°C) → Level 3
    if (latest_temp > 35.0f) {
        return BUZZER_CRITICAL_1S_10X;
    }
    
    // Priority 2: VERY POOR AIR (level 4) → Level 2
    if (latest_air_level >= 4) {
        return BUZZER_ALERT_2S_5X;
    }
    
    // Priority 3: HIGH TEMP (30-35°C) OR POOR AIR (level 3) → Level 1
    if (latest_temp > 30.0f || latest_air_level >= 3) {
        return BUZZER_WARN_5S;
    }
    
    // No alert
    return BUZZER_OFF;
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

static void lcd_task(void *pvParameters)
{
    (void) pvParameters;
    
    ESP_LOGI(TAG, "LCD task started");
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            sys_event_group, 
            EVT_SENSOR_READY, 
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(1000)
        );
        
        if (bits & EVT_SENSOR_READY) {
            lcd_display_all(
                latest_temp, 
                latest_humi, 
                latest_air_level, 
                latest_mq_ppm
            );
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));
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

            // ========== 🔔 4-LEVEL BUZZER SYSTEM ==========
            buzzer_level_t new_level = calculate_buzzer_level();
            
            // Chỉ kêu khi CHUYỂN LEVEL hoặc lặp lại theo chu kỳ
            if (new_level != current_buzzer_level) {
                // Level thay đổi → kêu ngay
                ESP_LOGI(TAG, "[BUZZER] Level changed: %d → %d", current_buzzer_level, new_level);
                current_buzzer_level = new_level;
                buzzer_cycle_count = 0;
                
                buzzer_play_pattern(current_buzzer_level);
            } 
            else if (current_buzzer_level != BUZZER_OFF) {
                // Level không đổi nhưng vẫn đang alert → xét lặp lại
                buzzer_cycle_count++;
                
                if (current_buzzer_level == BUZZER_CRITICAL_1S_10X) {
                    // Level 3: Lặp mỗi 3 chu kỳ sensor (~30 giây)
                    if (buzzer_cycle_count % 3 == 0) {
                        ESP_LOGE(TAG, "[BUZZER] CRITICAL REPEAT - Iteration %lu", buzzer_cycle_count / 3);
                        buzzer_play_pattern(BUZZER_CRITICAL_1S_10X);
                    }
                } 
                else if (current_buzzer_level == BUZZER_ALERT_2S_5X) {
                    // Level 2: Lặp mỗi 6 chu kỳ sensor (~60 giây)
                    if (buzzer_cycle_count % 6 == 0) {
                        ESP_LOGW(TAG, "[BUZZER] ALERT REPEAT - Iteration %lu", buzzer_cycle_count / 6);
                        buzzer_play_pattern(BUZZER_ALERT_2S_5X);
                    }
                }
                // Level 1 (WARN_5S): Chỉ kêu 1 lần, không lặp lại
            }
            
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
    lcd_init();

    // MQ135 Calibration
    ESP_LOGI(TAG, "Waiting 30 seconds for MQ135 warmup...");
    
    for (int i = 30; i > 0; i--) {
        // Display warmup countdown on LCD
        // Note: lcd_set_cursor and lcd_print are static functions in lcd1602.c
        // Use lcd_display_all or lcd_display instead
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (!mq135_has_calibration()) {
        ESP_LOGI(TAG, "Starting MQ135 calibration...");
        lcd_clear();
        // Note: Removed lcd_set_cursor and lcd_print calls as they are static functions
        // LCD will show init message from lcd_init()
        
        esp_err_t cal_result = mq135_calibrate();
        if (cal_result == ESP_OK) {
            ESP_LOGI(TAG, "✓ MQ135 calibration successful!");
            lcd_clear();
            vTaskDelay(pdMS_TO_TICKS(1500));
        } else {
            ESP_LOGW(TAG, "✗ MQ135 calibration failed");
            lcd_clear();
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    } else {
        ESP_LOGI(TAG, "MQ135 already calibrated");
        lcd_clear();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    wifi_init();
    mqtt_app_start(NULL);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
    xTaskCreate(actuator_task, "actuator_task", 3072, NULL, 6, &actuator_task_handle);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 4, &mqtt_task_handle);
    xTaskCreate(lcd_task, "lcd_task", 3072, NULL, 3, &lcd_task_handle);

    ESP_LOGI(TAG, "System initialized");
}