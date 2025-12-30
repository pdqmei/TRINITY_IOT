// Application configuration and pin mappings for TRINITY_IOT
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

// Sensor pins
#define SHT31_I2C_SDA_GPIO 21
#define SHT31_I2C_SCL_GPIO 22

// MQ135 ADC
#define MQ135_ADC_CHANNEL 0 // ADC1_CHANNEL_0 (GPIO36)

// Actuators
#define FAN_PWM_GPIO 16
#define BUZZER_GPIO 17
#define LED_R_GPIO 25
#define LED_G_GPIO 26
#define LED_B_GPIO 27

// Timing
#define SENSOR_READ_INTERVAL_MS 5000
#define MQTT_PUBLISH_INTERVAL_MS 10000

// LEDC PWM configuration
#define LEDC_FREQUENCY_HZ 5000
#define LEDC_RESOLUTION_BITS 10
#define LEDC_DUTY_MAX ((1 << LEDC_RESOLUTION_BITS) - 1)

// Fan specific
#define FAN_LEDC_TIMER    LEDC_TIMER_0
#define FAN_LEDC_CHANNEL  LEDC_CHANNEL_0
#define FAN_LEDC_MODE     LEDC_LOW_SPEED_MODE

// RGB LED uses a different timer
#define RGB_LEDC_TIMER    LEDC_TIMER_1

// Safety / ramping
#define FAN_RAMP_MS 500
#define FAN_MIN_SAFE_PERCENT 20

// MQTT
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#define MQTT_BROKER_PORT 1883

#endif // APP_CONFIG_H
