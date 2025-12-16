#include "device_control.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DEVICE";

// Biến lưu trạng thái LED
static int led_state = 0;

void device_init(void)
{
    // Cấu hình GPIO cho Buzzer và LED (OUTPUT đơn giản)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN) | (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Cấu hình PWM cho quạt (điều chỉnh tốc độ)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,  // 0-255
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = FAN_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
    
    // Tắt tất cả thiết bị ban đầu
    buzzer_off();
    led_off();
    fan_off();
    
    ESP_LOGI(TAG, "Devices initialized");
}

// ===== BUZZER =====
void buzzer_on(void) {
    gpio_set_level(BUZZER_PIN, 1);
    ESP_LOGI(TAG, "🔊 Buzzer ON");
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_PIN, 0);
    ESP_LOGI(TAG, "🔇 Buzzer OFF");
}

void buzzer_beep(int times) {
    for(int i = 0; i < times; i++) {
        buzzer_on();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        buzzer_off();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ===== LED =====
void led_on(void) {
    gpio_set_level(LED_PIN, 1);
    led_state = 1;
    ESP_LOGI(TAG, "💡 LED ON");
}

void led_off(void) {
    gpio_set_level(LED_PIN, 0);
    led_state = 0;
    ESP_LOGI(TAG, "💡 LED OFF");
}

void led_toggle(void) {
    led_state = !led_state;
    gpio_set_level(LED_PIN, led_state);
    ESP_LOGI(TAG, "💡 LED %s", led_state ? "ON" : "OFF");
}

// ===== FAN (PWM) =====
void fan_on(void) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255);  // 100%
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan ON (100%%)");
}

void fan_off(void) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan OFF");
}

void fan_speed(int speed) {
    // speed: 0-100 (%)
    if(speed < 0) speed = 0;
    if(speed > 100) speed = 100;
    
    int duty = (speed * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan speed: %d%%", speed);
}