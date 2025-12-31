/* Active buzzer controlled by GPIO (active LOW): LOW = ON, HIGH = OFF
 * Pin: PIN_BUZZER (see app_config.h)
 */

#include "buzzer.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUZZER_PIN PIN_BUZZER

void buzzer_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    // Active LOW -> set HIGH to keep buzzer off
    gpio_set_level(BUZZER_PIN, 1);
}

void buzzer_on(void) {
    gpio_set_level(BUZZER_PIN, 0); // active low
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_PIN, 1);
}

void buzzer_beep(int count) {
    for (int i = 0; i < count; i++) {
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
