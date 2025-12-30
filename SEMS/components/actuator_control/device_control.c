#include "device_control.h"
#include "esp_log.h"
#include "led.h"
#include "fan.h"
#include "whistle.h"

static const char *TAG = "ACTUATOR";

void device_init(void)
{
   
    led_init();
    fan_init();
    whistle_init();

    led_off();
    fan_off();
    whistle_off();

    ESP_LOGI(TAG, "All devices initialized");
}
void control_fan(bool state, int level)
{
    ESP_LOGI(TAG, "Fan %s level %d", state ? "ON" : "OFF", level);
}

void control_led(bool state, int level)
{
    ESP_LOGI(TAG, "LED %s level %d", state ? "ON" : "OFF", level);
}

void control_whistle(bool state, int level)
{
    ESP_LOGI(TAG, "Whistle %s level %d", state ? "ON" : "OFF", level);
}