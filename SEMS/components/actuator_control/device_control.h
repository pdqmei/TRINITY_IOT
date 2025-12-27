#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H
#include "driver/gpio.h"

// Định nghĩa GPIO cho các thiết bị
#define BUZZER_PIN      GPIO_NUM_25     // Còi 5V
#define LED_PIN         GPIO_NUM_26     // LED
#define FAN_PIN         GPIO_NUM_27     // Quạt DC (qua transistor)

// Khởi tạo tất cả thiết bị
void device_init(void);

// Include các module con để sử dụng
#include "fan.h"
#include "led.h"
#include "whistle.h"

#endif