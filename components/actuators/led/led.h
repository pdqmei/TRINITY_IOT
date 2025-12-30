#ifndef LED_H
#define LED_H

#include <stdint.h>

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
void led_blink(uint16_t interval_ms);

// Set RGB PWM duty (0-1023)
void led_set_rgb(uint16_t r, uint16_t g, uint16_t b);

#endif
