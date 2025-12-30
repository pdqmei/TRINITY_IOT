// Main application implementing sensor read, actuator control, MQTT publish
// Theo spec: SHT31 (I2C), MQ135 (ADC), Fan (digital), LED RGB (PWM), Buzzer active LOW

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

// Static buffers / moving averages (size 10)
static moving_average_t ma_temp;
static moving_average_t ma_humi;
static moving_average_t ma_air;

// Task handles
static TaskHandle_t sensor_task_handle = NULL;
static TaskHandle_t actuator_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;

// Latest averaged values (shared) - protected by task sync (single writer)
static float latest_temp = 0.0f;
static float latest_humi = 0.0f;
static int latest_air_level = 0; // 0-4

// Event group for signalling between tasks
static EventGroupHandle_t sys_event_group;
enum {
    EVT_SENSOR_READY = BIT0,
    EVT_WIFI_CONNECTED = BIT1,
    EVT_MQTT_CONNECTED = BIT2,
};

// Map ADC raw 0-4095 to air quality level per spec
static int map_adc_to_level(int raw)
{
    if (raw <= 819) return 0;
    if (raw <= 1638) return 1;
    if (raw <= 2457) return 2;
    if (raw <= 3276) return 3;
    return 4;
}

// Read SHT31 with retry (3 attempts). Return true if success.
static bool read_sht31_with_retry(sht31_data_t *out)
{
    for (int i = 0; i < 3; i++) {
        sht31_data_t d = sht31_read();
        // basic validation: temperature not NaN and in reasonable range
        if (d.temperature > -40 && d.temperature < 125) {
            *out = d;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

// Read MQ135 raw with retry
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

// sensor_task: period 5s, compute moving average, notify actuator and mqtt
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
            // use previous averages
        } else {
            ma_add(&ma_temp, sdata.temperature);
            ma_add(&ma_humi, sdata.humidity);
            latest_temp = ma_get(&ma_temp);
            latest_humi = ma_get(&ma_humi);
        }

        if (!mq_ok) {
            ESP_LOGE(TAG, "MQ135 read failed, using last value");
        } else {
            int level = map_adc_to_level(mq_raw);
            ma_add(&ma_air, (float)level);
            latest_air_level = (int)ma_get(&ma_air);
        }

        // Signal actuator task that new sensor data is available
        xEventGroupSetBits(sys_event_group, EVT_SENSOR_READY);

        // Sleep until next period
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

// actuator_task: event-driven (sensor updates). Controls fan, LED, buzzer
static void actuator_task(void *pvParameters)
{
    (void) pvParameters;

    // initial states
    fan_init();
    led_init();
    buzzer_init();

    while (1) {
        // Wait for sensor ready event
        EventBits_t bits = xEventGroupWaitBits(sys_event_group, EVT_SENSOR_READY, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & EVT_SENSOR_READY) {
            ESP_LOGI(TAG, "Actuator update: T=%.2f H=%.2f AQ=%d", latest_temp, latest_humi, latest_air_level);

            // Fan control: ON if >=25C, OFF otherwise
            if (latest_temp >= 25.0f) {
                fan_on();
                mqtt_send_data("status/fan", 1.0f);
            } else {
                fan_off();
                mqtt_send_data("status/fan", 0.0f);
            }

            // LED control based on air quality
            switch (latest_air_level) {
                case 0: // Green
                    led_set_rgb(0, 1023, 0);
                    break;
                case 1: // Yellow (R=255,G=255,B=0)
                    led_set_rgb(1023, 1023, 0);
                    break;
                case 2: // Orange (R=255,G=165,B=0) - map 165/255 ~ 662/1023
                    led_set_rgb(1023, 662, 0);
                    break;
                case 3: // Red blink 500ms
                    for (int i = 0; i < 2; i++) {
                        led_set_rgb(1023, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        led_set_rgb(0, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                    break;
                case 4: // Purple blink fast 200ms
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

            // Temperature overlay effects
            if (latest_temp > 35.0f) {
                // rapid beep and red blink
                for (int i = 0; i < 5; i++) {
                    buzzer_on();
                    led_set_rgb(1023, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    buzzer_off();
                    led_set_rgb(0, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            } else if (latest_temp > 30.0f) {
                // breathing red effect (fade in/out 2s)
                for (int step = 0; step <= 1023; step += 16) {
                    led_set_rgb(step, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(2000 / (1023/16 + 1)));
                }
                for (int step = 1023; step >= 0; step -= 16) {
                    led_set_rgb(step, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(2000 / (1023/16 + 1)));
                }
            }

            // Buzzer patterns
            if (latest_air_level >= 3) {
                // beep every 5s (100ms on/off) - implement single beep here
                buzzer_beep(1);
            }
            if (latest_temp > 35.0f) {
                // continuous rapid beep for a short burst
                for (int i = 0; i < 10; i++) {
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(50));
                    buzzer_off();
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
    }
}

// mqtt_task: publish sensor data every 10s
static void mqtt_task(void *pvParameters)
{
    (void) pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        // publish temperature/humidity/air_quality
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"C\"}", latest_temp);
        mqtt_send_data("sensor/temperature", latest_temp);

        snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"%%\"}", latest_humi);
        mqtt_send_data("sensor/humidity", latest_humi);

        // publish air quality as numeric value using mqtt helper
        mqtt_send_data("sensor/air_quality", (float)latest_air_level);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS));
    }
}

void app_main(void)
{
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "TRINITY_IOT - Environment Monitor starting");

    // init shared event group
    sys_event_group = xEventGroupCreate();

    // init peripherals (fan/led/buzzer/sensors)
    fan_init();
    led_init();
    buzzer_init();
    sht31_init();
    mq135_init();

    // WiFi
    wifi_init();
    // start MQTT after WiFi ideally, but here we start optimistic
    mqtt_app_start(NULL);

    // Create tasks
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
    xTaskCreate(actuator_task, "actuator_task", 3072, NULL, 6, &actuator_task_handle);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 4, &mqtt_task_handle);

    ESP_LOGI(TAG, "System initialized");
}
