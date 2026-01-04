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
#define FAN_LEDC_FREQ_HZ    1000  // Lower frequency for IRF520 module
#define FAN_LEDC_CHANNEL    LEDC_CHANNEL_3
#define FAN_PIN             PIN_FAN

// Set to 1 if IRF520 module has inverted logic (HIGH=OFF, LOW=ON)
#define FAN_INVERT_OUTPUT   1

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
        .flags.output_invert = FAN_INVERT_OUTPUT,
    };
    ledc_channel_config(&ledc_channel);
}

void fan_init(void) {
    // First, configure GPIO as output and set to LOW before LEDC init
    // This prevents glitches during initialization
    gpio_reset_pin(FAN_PIN);
    gpio_set_direction(FAN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(FAN_PIN, GPIO_PULLDOWN_ONLY);  // Enable internal pull-down
    gpio_set_level(FAN_PIN, 0);  // Ensure LOW (MOSFET OFF)
    
    // Now configure LEDC
    fan_ledc_init();
    
    // Ensure fan off
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, 0);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
    ledc_stop(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, 0);
}

void fan_on(void) {
    // Restart PWM in case it was stopped by fan_off()
    ledc_timer_resume(FAN_LEDC_MODE, FAN_LEDC_TIMER);
    
    // Set to max duty (level 3)
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, fan_duties[3]);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}

void fan_off(void) {
    // Set duty to 0 first
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, 0);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
    
    // Force stop PWM and set GPIO to LOW (idle_level = 0)
    ledc_stop(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, 0);
    
    // Explicitly set GPIO to LOW to ensure MOSFET gate is fully discharged
    gpio_set_level(FAN_PIN, 0);
}

void fan_set_speed(uint8_t speed) {
    // speed: 0-100 -> map to 4 levels
    if (speed == 0) {
        fan_off();
        return;
    }
    
    // Restart PWM in case it was stopped
    ledc_timer_resume(FAN_LEDC_MODE, FAN_LEDC_TIMER);
    
    // Determine level 1..3
    uint8_t level = 1;
    if (speed <= 33) level = 1;
    else if (speed <= 66) level = 2;
    else level = 3;

    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, fan_duties[level]);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}