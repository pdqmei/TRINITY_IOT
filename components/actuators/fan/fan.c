/*
 * Fan driver adapted for simple ON/OFF control via GPIO (MOSFET gate).
 * Hardware: Fan controlled through MOSFET on GPIO18. Fan does not support PWM.
 * GPIO18 HIGH -> fan ON (MOSFET gate driven), LOW -> fan OFF.
 * fan_set_speed() treats any non-zero speed as ON.
 */

/*
 * Fan driver with 4 discrete PWM levels using LEDC.
 * Hardware: Fan controlled through MOSFET on configured PIN_FAN (see app_config.h).
 * Levels: 0..3 (4 levels). fan_set_speed(0-100) maps to nearest level.
 */

#include "fan.h"
#include "app_config.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#define FAN_LEDC_TIMER      LEDC_TIMER_1
#define FAN_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define FAN_LEDC_RES        LEDC_TIMER_10_BIT
#define FAN_LEDC_FREQ_HZ    5000
#define FAN_LEDC_CHANNEL    LEDC_CHANNEL_3
#define FAN_PIN             PIN_FAN

// Discrete duty levels for 10-bit resolution (0-1023)
static const uint32_t fan_duties[4] = {0, 341, 682, 1023};

static void fan_ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = FAN_LEDC_MODE,
        .duty_resolution = FAN_LEDC_RES,
        .timer_num = FAN_LEDC_TIMER,
        .freq_hz = FAN_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = FAN_LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = FAN_PIN,
        .speed_mode = FAN_LEDC_MODE,
        .hpoint = 0,
        .timer_sel = FAN_LEDC_TIMER,
        .flags.output_invert = 0,
    };
    ledc_channel_config(&ledc_channel);
}

void fan_init(void) {
    // configure pin via ledc channel
    fan_ledc_init();
    // ensure fan off
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, 0);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}

void fan_on(void) {
    // set to max duty (level 3)
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, fan_duties[3]);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}

void fan_off(void) {
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, fan_duties[0]);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}

void fan_set_speed(uint8_t speed) {
    // speed: 0-100 -> map to 4 levels
    if (speed == 0) {
        fan_off();
        return;
    }
    // Determine level 1..3
    uint8_t level = 1;
    if (speed <= 33) level = 1;
    else if (speed <= 66) level = 2;
    else level = 3;

    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, fan_duties[level]);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}
