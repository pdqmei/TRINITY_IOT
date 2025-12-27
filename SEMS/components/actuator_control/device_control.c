#include "device_control.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "led.h"
#include "fan.h"
#include "whistle.h"

static const char *TAG = "ACTUATOR";

// Biến lưu trạng thái LED
static int led_state = 0;

void device_init(void)
{
    // Cấu hình GPIO cho Buzzer và LED 
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
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT, // 0-255
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = FAN_PIN,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);

    whistle_off();
    led_off();
    fan_off();

    ESP_LOGI(TAG, "Devices initialized");
}
