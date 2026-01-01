#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

void mq135_init(void);
uint16_t mq135_read_raw(void);
float mq135_read_ppm(void);
uint8_t mq135_get_air_quality_level(void);

// Calibration functions
esp_err_t mq135_calibrate(void);
bool mq135_has_calibration(void);
esp_err_t mq135_clear_calibration(void);

void mq135_deinit(void);

#endif // MQ135_H