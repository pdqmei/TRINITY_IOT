#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"

static int led_state = 0;

static const char *TAG = "LED";

void led_on(void)
{
    gpio_set_level(LED_PIN, 1);
    led_state = 1;
    ESP_LOGI(TAG, "💡 LED ON");
}

void led_off(void)
{
    gpio_set_level(LED_PIN, 0);
    led_state = 0;
    ESP_LOGI(TAG, "💡 LED OFF");
}

void led_toggle(void)
{
    led_state = !led_state;
    gpio_set_level(LED_PIN, led_state);
    ESP_LOGI(TAG, "💡 LED %s", led_state ? "ON" : "OFF");
}