#include "fan.h"
#include "device_control.h"
#include "driver/gpio.h"

void fan_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FAN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void fan_on(void) {
    gpio_set_level(FAN_PIN, 1);
}

void fan_off(void) {
    gpio_set_level(FAN_PIN, 0);
}
