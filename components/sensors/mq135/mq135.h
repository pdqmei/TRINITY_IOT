#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdbool.h>
#include "esp_err.h"

// Simple MQ135 interface
void mq135_init(void);
uint16_t mq135_read_raw(void);
float mq135_read_ppm(void); // returns -1.0 on error/uncalibrated
uint8_t mq135_get_air_quality_level(void);

// Calibration helpers
// Perform calibration in known clean air (blocks briefly to sample)
esp_err_t mq135_calibrate(void);
// Returns true if calibration was loaded / set
bool mq135_has_calibration(void);
// Remove stored calibration
esp_err_t mq135_clear_calibration(void);
void mq135_deinit(void);

#endif // MQ135_H