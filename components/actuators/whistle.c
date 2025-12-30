/* Active buzzer controlled by GPIO (active LOW): LOW = ON, HIGH = OFF
 * Pin: GPIO23
 */

#include "whistle.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WHISTLE_PIN GPIO_NUM_23

void whistle_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WHISTLE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    // Active LOW -> set HIGH to keep buzzer off
    gpio_set_level(WHISTLE_PIN, 1);
}

void whistle_on(void) {
    gpio_set_level(WHISTLE_PIN, 0); // active low
}

void whistle_off(void) {
    gpio_set_level(WHISTLE_PIN, 1);
}

void whistle_beep(int count) {
    for (int i = 0; i < count; i++) {
        whistle_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        whistle_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
