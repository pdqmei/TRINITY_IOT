#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "driver/gpio.h"

// Định nghĩa GPIO cho các thiết bị
#define BUZZER_PIN      GPIO_NUM_25     // Còi 5V
#define LED_PIN         GPIO_NUM_26     // LED
#define FAN_PIN         GPIO_NUM_27     // Quạt DC (qua transistor)

// Khởi tạo thiết bị
void device_init(void);

// Điều khiển thiết bị
void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep(int times);        // Kêu bíp nhiều lần

void led_on(void);
void led_off(void);
void led_toggle(void);              // Đảo trạng thái LED

void fan_on(void);
void fan_off(void);
void fan_speed(int speed);          // Điều chỉnh tốc độ (PWM)

#endif