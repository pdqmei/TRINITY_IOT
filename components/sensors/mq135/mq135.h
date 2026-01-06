#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/*
 * MQ-135 Air Quality Sensor Driver
 *
 * IMPORTANT:
 * - Sensor must be powered at 5V (heater + module)
 * - First use requires 24–48h preheat
 * - Calibration must be done in clean air
 * - ppm value is ESTIMATED (CO2 equivalent), not laboratory accurate
 */

// ========== INIT / DEINIT ==========
void mq135_init(void);
void mq135_deinit(void);

// ========== BASIC READ ==========
/**
 * @brief Check if MQ135 sensor is connected
 * @return true if sensor is connected and responding
 */
bool mq135_is_connected(void);

/**
 * @brief Read raw ADC value (0–4095)
 * @return Raw ADC value
 */
uint16_t mq135_read_raw(void);

// ========== GAS ESTIMATION ==========
/**
 * @brief Read estimated CO2 equivalent concentration (ppm)
 *
 * @return
 *   - ppm value (estimated) if calibrated
 *   - -1.0f if not calibrated or error
 */
float mq135_read_ppm(void);

// ========== AIR QUALITY INDEX (RELATIVE) ==========
/**
 * @brief Get air quality level (relative index)
 *
 * Levels:
 *   0 = Good
 *   1 = Fair
 *   2 = Moderate
 *   3 = Poor
 *   4 = Very Poor
 *
 * @return Air quality level
 */
uint8_t mq135_get_air_quality_level(void);

// ========== CALIBRATION ==========
/**
 * @brief Calibrate MQ-135 in clean air (~400ppm CO2)
 *
 * This function:
 *   - Measures sensor resistance in clean air
 *   - Calculates Ro using datasheet clean air factor
 *   - Saves Ro to NVS (persistent)
 *
 * @return ESP_OK on success
 */
esp_err_t mq135_calibrate(void);

/**
 * @brief Check if sensor has valid calibration data
 * @return true if calibrated
 */
bool mq135_has_calibration(void);

/**
 * @brief Clear calibration data (RAM + NVS)
 * @return ESP_OK
 */
esp_err_t mq135_clear_calibration(void);

#endif // MQ135_H
