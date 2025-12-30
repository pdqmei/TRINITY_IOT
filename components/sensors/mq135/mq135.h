#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>

void mq135_init(void);
uint16_t mq135_read_raw(void);
float mq135_read_ppm(void);
uint8_t mq135_get_air_quality_level(void);

#endif
