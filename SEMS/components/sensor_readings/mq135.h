#ifndef MQ135_H
#define MQ135_H

#include "hal/adc_types.h"

void mq135_init(void);
int mq135_read_raw(void);
int mq135_read_voltage(void);

#endif
