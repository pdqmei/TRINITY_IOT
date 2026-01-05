#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>

// Basic buzzer control
void buzzer_init(void);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep(int count);

// Pattern-based buzzer control (non-blocking, uses FreeRTOS task)
// Level 0: OFF
// Level 1: Tít ngắn, nghỉ 5 giây
// Level 2: Tít ngắn, nghỉ 3 giây  
// Level 3: Tít ngắn, nghỉ 1 giây
void buzzer_set_level(int level);
int buzzer_get_level(void);
bool buzzer_is_running(void);

#endif
