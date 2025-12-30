#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// Simple MQ135 interface
void mq135_init(void);
uint16_t mq135_read_raw(void);
float mq135_read_ppm(void);
uint8_t mq135_get_air_quality_level(void);
void mq135_deinit(void);

#endif // MQ135_H