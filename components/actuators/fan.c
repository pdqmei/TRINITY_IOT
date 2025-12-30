/*
 * Fan driver adapted for simple ON/OFF control via GPIO (MOSFET gate).
 * Hardware: Fan controlled through MOSFET on GPIO18. Fan does not support PWM.
 * GPIO18 HIGH -> fan ON (MOSFET gate driven), LOW -> fan OFF.
 * fan_set_speed() treats any non-zero speed as ON.
 */

#include "fan.h"
#include "driver/gpio.h"

#define FAN_PIN GPIO_NUM_18

void fan_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FAN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Ensure fan is off at startup
    gpio_set_level(FAN_PIN, 0);
}

void fan_on(void) {
    gpio_set_level(FAN_PIN, 1);
}

void fan_off(void) {
    gpio_set_level(FAN_PIN, 0);
}

void fan_set_speed(uint8_t speed) {
    // Fan is digital only; map >0 to ON, 0 to OFF
    if (speed == 0) fan_off();
    else fan_on();
}
