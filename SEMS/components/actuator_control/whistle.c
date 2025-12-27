#include "whistle.h"
#include "device_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WHISTLE";

void whistle_on(void)
{
    gpio_set_level(BUZZER_PIN, 1);
    ESP_LOGI(TAG, "🔊 Whistle ON");
}

void whistle_off(void)
{
    gpio_set_level(BUZZER_PIN, 0);
    ESP_LOGI(TAG, "🔇 Whistle OFF");
}

void whistle_beep(int times)
{
    for (int i = 0; i < times; i++)
    {
        whistle_on();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        whistle_off();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}