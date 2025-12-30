#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdbool.h>

void fan_init(void);
void fan_set_speed_percent(int percent); // 0-100
void fan_on(void);
void fan_off(void);

#endif
