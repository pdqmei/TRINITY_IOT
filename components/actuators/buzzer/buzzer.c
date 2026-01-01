/* Active buzzer controlled by GPIO (active LOW): LOW = ON, HIGH = OFF
 * Pin: PIN_BUZZER (see app_config.h)
 */

#include "buzzer.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BUZZER";

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
    
    ESP_LOGI(TAG, "Buzzer initialized on pin %d (active LOW)", BUZZER_PIN);
}

void buzzer_on(void) {
    gpio_set_level(BUZZER_PIN, 0); // active low
    ESP_LOGI(TAG, "Buzzer ON");
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_PIN, 1);
    ESP_LOGI(TAG, "Buzzer OFF");
}

void buzzer_beep(int count) {
    ESP_LOGI(TAG, "Buzzer beep %d time(s)", count);
    for (int i = 0; i < count; i++) {
        ESP_LOGD(TAG, "Beep #%d", i+1);
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "Buzzer beep completed");
}