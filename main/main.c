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
#include "driver/gpio.h"

static const char *TAG = "MAIN";

// GPIO for MQTT status LED (Red LED through 220Ω resistor)
#define MQTT_STATUS_LED_GPIO 18

// ---TOPIC MQTT ---
// ID DYNAMIC for room 
char device_room_id[32] = "livingroom"; 

// Các biến lưu Topic sau khi đã ghép chuỗi
char topic_fan[128];
char topic_led[128];
char topic_buzzer[128];
char topic_temp[128];
char topic_humi[128];
char topic_co2[128];

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
static bool sht31_valid = false;  // Trạng thái cảm biến SHT31

// ========== 4-LEVEL BUZZER SYSTEM ==========
// Buzzer levels: 0=OFF, 1=WARN(5s), 2=ALERT(2sx5), 3=CRITICAL(1sx10)
typedef enum {
    BUZZER_OFF = 0,           // Tắt
    BUZZER_WARN_5S = 1,       // Cảnh báo: kêu liên tục 5 giây
    BUZZER_ALERT_2S_5X = 2,   // Nguy hiểm: kêu 2s, lặp 5 lần
    BUZZER_CRITICAL_1S_10X = 3 // Nghiêm trọng: kêu 1s, lặp 10 lần liên tục
} buzzer_level_t;

static buzzer_level_t current_buzzer_level = BUZZER_OFF;

// NOTE: buzzer_task_handle và buzzer_pattern_level được quản lý trong main.c
// mqtt_handler.c có bản sao riêng cho MANUAL mode
static TaskHandle_t buzzer_task_handle = NULL;
static volatile int buzzer_pattern_level = 0;  // Shared with buzzer task

// Fan control with hysteresis
static uint8_t current_fan_speed = 0;

// Event group
static EventGroupHandle_t sys_event_group;
enum {
    EVT_SENSOR_READY = BIT0,
    EVT_WIFI_CONNECTED = BIT1,
    EVT_MQTT_CONNECTED = BIT2,
    EVT_MODE_CHANGED = BIT3,  // Trigger khi chuyển AUTO mode
};

// ===============================================
// HÀM TRIGGER ACTUATOR UPDATE (GỌI TỪ mqtt_handler.c)
// ===============================================
void trigger_actuator_update(void)
{
    if (sys_event_group != NULL) {
        xEventGroupSetBits(sys_event_group, EVT_MODE_CHANGED);
        ESP_LOGI(TAG, "🔄 Triggered actuator update for AUTO mode");
    }
}

void setup_mqtt_topics(const char* room_id)
{
    // home/[ROOM_ID]/actuators/fan
    snprintf(topic_fan, sizeof(topic_fan), "smarthome/%s/actuators/fan", room_id);
    snprintf(topic_led, sizeof(topic_led), "smarthome/%s/actuators/led", room_id);
    snprintf(topic_buzzer, sizeof(topic_buzzer), "smarthome/%s/actuators/buzzer", room_id);

    // home/[ROOM_ID]/sensors/temp
    snprintf(topic_temp, sizeof(topic_temp), "smarthome/%s/sensors/temp", room_id);
    snprintf(topic_humi, sizeof(topic_humi), "smarthome/%s/sensors/humi", room_id);
    snprintf(topic_co2, sizeof(topic_co2), "smarthome/%s/sensors/co2", room_id);
}

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

// ========== BUZZER PATTERN TASK (LOOPING CONTINUOUSLY) ==========
static void buzzer_pattern_task(void *pvParameters)
{
    int level = buzzer_pattern_level;
    ESP_LOGI(TAG, "[BUZZER] Task started - Level %d (looping)", level);
    
    while (1) {
        if (buzzer_pattern_level == 0) break;
        
        switch (level) {
            case 1:  // Level 1: Kêu 5s, nghỉ 2s, lặp lại
                buzzer_on();
                for (int i = 0; i < 50 && buzzer_pattern_level == level; i++)
                    vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_off();
                for (int i = 0; i < 20 && buzzer_pattern_level == level; i++)
                    vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case 2:  // Level 2: Kêu 2s, tắt 0.5s x5, nghỉ 2s, lặp lại
                for (int cycle = 0; cycle < 5 && buzzer_pattern_level == level; cycle++) {
                    buzzer_on();
                    for (int i = 0; i < 20 && buzzer_pattern_level == level; i++)
                        vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    for (int i = 0; i < 5 && buzzer_pattern_level == level; i++)
                        vTaskDelay(pdMS_TO_TICKS(100));
                }
                for (int i = 0; i < 20 && buzzer_pattern_level == level; i++)
                    vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case 3:  // Level 3: Kêu 1s, tắt 0.3s x10, nghỉ 1s, lặp lại
            default:
                for (int cycle = 0; cycle < 10 && buzzer_pattern_level == level; cycle++) {
                    buzzer_on();
                    for (int i = 0; i < 10 && buzzer_pattern_level == level; i++)
                        vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    for (int i = 0; i < 3 && buzzer_pattern_level == level; i++)
                        vTaskDelay(pdMS_TO_TICKS(100));
                }
                for (int i = 0; i < 10 && buzzer_pattern_level == level; i++)
                    vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
        
        if (buzzer_pattern_level != level) {
            level = buzzer_pattern_level;
            if (level == 0) break;
            ESP_LOGI(TAG, "[BUZZER] Level changed to %d", level);
        }
    }
    
    buzzer_off();
    ESP_LOGI(TAG, "[BUZZER] Task stopped");
    buzzer_task_handle = NULL;
    vTaskDelete(NULL);
}

// Khởi chạy/dừng buzzer pattern
static void buzzer_start_pattern(int level)
{
    if (level == buzzer_pattern_level && buzzer_task_handle != NULL) {
        return;  // Đang chạy đúng level
    }
    
    buzzer_pattern_level = level;
    
    if (level <= 0) {
        buzzer_off();
        ESP_LOGI(TAG, "[BUZZER] Level 0: OFF");
        return;
    }
    
    if (buzzer_task_handle == NULL) {
        xTaskCreate(buzzer_pattern_task, "buzzer_auto", 2048, NULL, 5, &buzzer_task_handle);
        ESP_LOGW(TAG, "[BUZZER] Started level %d (continuous)", level);
    } else {
        ESP_LOGW(TAG, "[BUZZER] Changed to level %d", level);
    }
}

static buzzer_level_t calculate_buzzer_level(void)
{
    // Priority 1: CRITICAL HUMIDITY (> 80%) → Level 3
    if (latest_humi > 80.0f) {
        return BUZZER_CRITICAL_1S_10X;
    }
    
    // Priority 2: VERY POOR AIR QUALITY (level 4) → Level 2
    if (latest_air_level >= 3) {
        return BUZZER_ALERT_2S_5X;
    }
    
    // Priority 3: HIGH HUMIDITY (> 70%) → Level 1
    if (latest_humi > 70.0f) {
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
            ESP_LOGE(TAG, "SHT31 read failed, sensor disconnected");
            sht31_valid = false;
        } else {
            ma_add(&ma_temp, sdata.temperature);
            ma_add(&ma_humi, sdata.humidity);
            latest_temp = ma_get(&ma_temp);
            latest_humi = ma_get(&ma_humi);
            sht31_valid = true;
        }

        if (!mq_ok) {
            ESP_LOGE(TAG, "MQ135 read failed, using last value");
        } else {
            float ppm = mq135_read_ppm();
            
            // AQ level dựa trên ADC raw (ổn định hơn ppm)
            int level;
            if (mq_raw < 600) level = 0;          // Clean
            else if (mq_raw < 900) level = 1;     // Fair
            else if (mq_raw < 1300) level = 2;    // Moderate
            else if (mq_raw < 1800) level = 3;    // Poor
            else level = 4;                       // Very Poor
            
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

    // Actuators already initialized in app_main(), no need to init again

    while (1) {
        // Chờ event từ sensor hoặc mode change
        EventBits_t bits = xEventGroupWaitBits(
            sys_event_group, 
            EVT_SENSOR_READY | EVT_MODE_CHANGED,  // Lắng nghe cả 2 event
            pdTRUE,  // Clear bits sau khi đọc
            pdFALSE, // Chỉ cần 1 trong 2 event
            portMAX_DELAY
        );
        
        if (bits & (EVT_SENSOR_READY | EVT_MODE_CHANGED)) {

            if (!mqtt_is_auto_mode()) {
                ESP_LOGI(TAG, "👤 MANUAL mode - Skipping auto control");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            // ✅ Nếu vừa chuyển sang AUTO, reset hysteresis state
            if (bits & EVT_MODE_CHANGED) {
                current_fan_speed = 0;  // Reset để tính lại từ nhiệt độ hiện tại
                current_buzzer_level = BUZZER_OFF;
                ESP_LOGI(TAG, "🔄 Mode changed - Reset hysteresis state");
            }

            ESP_LOGI(TAG, "🤖 AUTO mode - Running auto control");
            ESP_LOGI(TAG, "===== Actuator Control Start =====");
            ESP_LOGI(TAG, "Sensors: T=%.2f°C, H=%.2f%%, AQ=%d", latest_temp, latest_humi, latest_air_level);

            // ========== FAN CONTROL WITH HYSTERESIS ==========
            uint8_t fan_speed = 0;

            if (!sht31_valid) {
                // Không có cảm biến SHT31 -> TẮT QUẠT
                fan_speed = 0;
                if (current_fan_speed != 0) {
                    ESP_LOGW(TAG, "[FAN] OFF (SHT31 không hoạt động)");
                    fan_off();
                }
            }
            else {
                // Hysteresis: Tránh bật/tắt liên tục khi nhiệt độ dao động
                // OFF zone: T < 24°C (hysteresis thấp hơn 25°C)
                // 50% zone: 25°C ≤ T < 29°C (hysteresis thấp hơn 30°C)
                // 100% zone: T ≥ 30°C
                
                if (current_fan_speed == 0) {
                    // Quạt đang TẮT → Bật khi T ≥ 25°C
                    if (latest_temp >= 25.0f) {
                        fan_speed = 50;
                        ESP_LOGI(TAG, "[FAN] 50%% (T=%.1f°C reached 25°C)", latest_temp);
                        fan_set_speed(fan_speed);
                    } else {
                        fan_speed = 0;
                    }
                }
                else if (current_fan_speed == 50) {
                    // Quạt đang 50% → Kiểm tra hysteresis
                    if (latest_temp >= 30.0f) {
                        // Tăng lên 100%
                        fan_speed = 100;
                        ESP_LOGI(TAG, "[FAN] 100%% (T=%.1f°C reached 30°C)", latest_temp);
                        fan_on();
                    } 
                    else if (latest_temp < 24.0f) {
                        // Giảm xuống OFF (hysteresis -1°C)
                        fan_speed = 0;
                        ESP_LOGI(TAG, "[FAN] OFF (T=%.1f°C dropped below 24°C)", latest_temp);
                        fan_off();
                    } 
                    else {
                        // Giữ nguyên 50%
                        fan_speed = 50;
                    }
                }
                else if (current_fan_speed == 100) {
                    // Quạt đang 100% → Kiểm tra hysteresis
                    if (latest_temp < 29.0f) {
                        // Giảm xuống 50% (hysteresis -1°C)
                        fan_speed = 50;
                        ESP_LOGI(TAG, "[FAN] 50%% (T=%.1f°C dropped below 29°C)", latest_temp);
                        fan_set_speed(fan_speed);
                    } 
                    else {
                        // Giữ nguyên 100%
                        fan_speed = 100;
                    }
                }
            }
            
            current_fan_speed = fan_speed;  // Lưu trạng thái
            // Fan level: 0=OFF, 1=50%, 2=100% (match MANUAL mode)
            int fan_level = (fan_speed == 0) ? 0 : ((fan_speed == 50) ? 1 : 2);
            mqtt_publish_actuator(topic_fan, (fan_speed > 0) ? "ON" : "OFF", fan_level);

            // ========== LED CONTROL ==========
            // LED Colors by Air Quality Level (match MANUAL mode):
            // Level 0: Green      (0, 1023, 0)     - Good
            // Level 1: Cyan       (0, 1023, 1023)  - Fair
            // Level 2: Yellow     (1023, 1023, 0)  - Moderate
            // Level 3: Red        (1023, 0, 0)     - Poor
            // Level 4: Purple     (1023, 0, 1023)  - Very Poor
            const char* aq_desc[] = {"Good", "Fair", "Moderate", "Poor", "Very Poor"};
            ESP_LOGI(TAG, "[LED] Air Quality: %s (level %d)", aq_desc[latest_air_level], latest_air_level);
            
            switch (latest_air_level) {
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
            mqtt_publish_actuator(topic_led, 
                     (latest_air_level >= 0 && latest_air_level <= 4) ? "ON" : "OFF", latest_air_level);

            // ========== 🔔 4-LEVEL BUZZER SYSTEM (CONTINUOUS) ==========
            buzzer_level_t new_level = calculate_buzzer_level();
            
            if (new_level != current_buzzer_level) {
                ESP_LOGI(TAG, "[BUZZER] Level changed: %d → %d", current_buzzer_level, new_level);
                current_buzzer_level = new_level;
                buzzer_start_pattern((int)new_level);  // Task tự lặp liên tục
            }
            
            mqtt_publish_actuator(topic_buzzer, 
                     (current_buzzer_level != BUZZER_OFF) ? "ON" : "OFF", (int)current_buzzer_level);
            
            ESP_LOGI(TAG, "===== Actuator Control End =====");
        }
    }
}

static void mqtt_task(void *pvParameters)
{
    (void) pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        // Update MQTT status LED
        bool mqtt_connected = mqtt_is_connected();
        gpio_set_level(MQTT_STATUS_LED_GPIO, mqtt_connected ? 1 : 0);

        // ========== LCD UPDATE (đồng bộ với MQTT) ==========
        lcd_display_all(
            latest_temp, 
            latest_humi, 
            latest_air_level, 
            latest_mq_ppm
        );

        // ========== MQTT PUBLISH ==========
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"C\"}", latest_temp);
        mqtt_send_data(topic_temp, latest_temp);

        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"%%\"}", latest_humi);
        mqtt_send_data(topic_humi, latest_humi);

        ESP_LOGI(TAG, "MQ135 last raw=%d PPM=%.2f level=%d", latest_mq_raw, latest_mq_ppm, latest_air_level);
        mqtt_send_data(topic_co2, latest_mq_ppm);

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

    // Initialize MQTT status LED (GPIO 18)
    gpio_reset_pin(MQTT_STATUS_LED_GPIO);
    gpio_set_direction(MQTT_STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(MQTT_STATUS_LED_GPIO, 0);  // Initially OFF
    ESP_LOGI(TAG, "MQTT status LED initialized on GPIO %d", MQTT_STATUS_LED_GPIO);

    // Setup MQTT topics
    setup_mqtt_topics(device_room_id);

    // MQ135 Calibration
    ESP_LOGI(TAG, "Waiting 10 seconds for MQ135 warmup...");
    
    for (int i = 10; i > 0; i--) {
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
    
    // ĐỢI WiFi kết nối trước khi khởi động MQTT
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    int wifi_timeout = 0;
    while (!wifi_is_connected() && wifi_timeout < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_timeout++;
        ESP_LOGI(TAG, "WiFi connecting... %d/30s", wifi_timeout);
    }
    
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "✓ WiFi connected! Starting MQTT...");
        mqtt_app_start(NULL);
    } else {
        ESP_LOGE(TAG, "✗ WiFi connection timeout! MQTT will retry later...");
        mqtt_app_start(NULL);  // MQTT sẽ tự retry khi có WiFi
    }

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
    xTaskCreate(actuator_task, "actuator_task", 3072, NULL, 6, &actuator_task_handle);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 4, &mqtt_task_handle);

    ESP_LOGI(TAG, "System initialized");
}