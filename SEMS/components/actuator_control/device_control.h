#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "driver/gpio.h"

#define BUZZER_PIN      GPIO_NUM_25     // Còi 5V
#define LED_PIN         GPIO_NUM_26     // LED
#define FAN_PIN         GPIO_NUM_27     // Quạt DC (qua transistor)

void device_init(void);



#endif