#ifndef FAN_H
#define FAN_H

#include <stdint.h>

void fan_init(void);
void fan_on(void);
void fan_off(void);
void fan_set_speed(uint8_t speed); // digital mapping: 0=off, >0=on

#endif
