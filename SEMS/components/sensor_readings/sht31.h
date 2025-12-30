#ifndef SHT31_H
#define SHT31_H

#include <stdbool.h>

bool sht31_init(void);
bool sht31_read(float *temperature, float *humidity);

#endif
