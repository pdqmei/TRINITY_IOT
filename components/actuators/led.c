/*
 * LED driver converted to use LEDC PWM on three channels for RGB
 * Pins:
 *  - Red:   GPIO25 (LEDC_CHANNEL_0)
 *  - Green: GPIO26 (LEDC_CHANNEL_1)
 *  - Blue:  GPIO27 (LEDC_CHANNEL_2)
 * PWM: 5 kHz, 10-bit resolution
 */

#include "led.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY      5000

#define LED_RED_GPIO        GPIO_NUM_25
#define LED_GREEN_GPIO      GPIO_NUM_26
#define LED_BLUE_GPIO       GPIO_NUM_27

#define LEDC_CH_RED         LEDC_CHANNEL_0
#define LEDC_CH_GREEN       LEDC_CHANNEL_1
#define LEDC_CH_BLUE        LEDC_CHANNEL_2

static uint8_t led_state = 0; // 0 = off, 1 = on

static void ledc_init_channels(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t red_cfg = {
        .gpio_num = LED_RED_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CH_RED,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&red_cfg);

    ledc_channel_config_t green_cfg = red_cfg;
    green_cfg.gpio_num = LED_GREEN_GPIO;
    green_cfg.channel = LEDC_CH_GREEN;
    ledc_channel_config(&green_cfg);

    ledc_channel_config_t blue_cfg = red_cfg;
    blue_cfg.gpio_num = LED_BLUE_GPIO;
    blue_cfg.channel = LEDC_CH_BLUE;
    ledc_channel_config(&blue_cfg);
}

void led_init(void) {
    ledc_init_channels();
    led_off();
}

void led_set_rgb(uint16_t r, uint16_t g, uint16_t b) {
    // r,g,b expected 0-1023 (10-bit)
    ledc_set_duty(LEDC_MODE, LEDC_CH_RED, r);
    ledc_update_duty(LEDC_MODE, LEDC_CH_RED);

    ledc_set_duty(LEDC_MODE, LEDC_CH_GREEN, g);
    ledc_update_duty(LEDC_MODE, LEDC_CH_GREEN);

    ledc_set_duty(LEDC_MODE, LEDC_CH_BLUE, b);
    ledc_update_duty(LEDC_MODE, LEDC_CH_BLUE);
}

void led_on(void) {
    // turn on to white full brightness
    led_set_rgb(1023, 1023, 1023);
    led_state = 1;
}

void led_off(void) {
    led_set_rgb(0, 0, 0);
    led_state = 0;
}

void led_toggle(void) {
    if (led_state) led_off();
    else led_on();
}

void led_blink(uint16_t interval_ms) {
    for (int i = 0; i < 3; i++) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
}
