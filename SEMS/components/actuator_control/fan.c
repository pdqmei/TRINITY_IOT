#include "fan.h"
#include "driver/gpio.h"
#include "esp_log.h"


static const char *TAG = "FAN"; 

void fan_on(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan ON (100%%)");
}

void fan_off(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan OFF");
}

void fan_speed(int speed)
{
    if (speed < 0)
        speed = 0;
    if (speed > 100)
        speed = 100;

    int duty = (speed * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "🌀 Fan speed: %d%%", speed);
}