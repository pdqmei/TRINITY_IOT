#include "mq135.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>

#define MQ135_PIN ADC_CHANNEL_0
#define MQ135_ATTEN ADC_ATTEN_DB_12
#define MQ135_WIDTH ADC_BITWIDTH_12

// MQ135 constants (from datasheet approximation)
#define RL_VALUE 10.0f           // Load resistance in kOhm (10kΩ typical)
#define VCC 3.3f                 // Supply voltage
#define ATMOCO2 400.0f          // Atmospheric CO2 level (ppm) for clean air

// Curve fitting parameters for CO2 (from datasheet)
// ppm = a * (Rs/Ro)^b
#define PARA_A 116.6020682f     // Approximation for CO2
#define PARA_B -2.769034857f    // Approximation for CO2

// NVS storage keys
#define NVS_NAMESPACE "mq135"
#define NVS_KEY_RO "ro_value"

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static float calibration_Ro = 0.0f;  // Ro in kOhm (resistance at 400ppm CO2)
static bool is_calibrated = false;

// ========== NVS FUNCTIONS ==========
static esp_err_t save_ro_to_nvs(float ro_value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_RO, &ro_value, sizeof(float));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "Ro saved to NVS: %.2f kΩ", ro_value);
    } else {
        ESP_LOGE(TAG, "Failed to save Ro: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

static esp_err_t load_ro_from_nvs(float *ro_value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not found (first run?)");
        return err;
    }

    size_t required_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, NVS_KEY_RO, ro_value, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Ro loaded from NVS: %.2f kΩ", *ro_value);
    } else {
        ESP_LOGW(TAG, "No calibration found in NVS");
    }

    nvs_close(nvs_handle);
    return err;
}

// ========== ADC FUNCTIONS ==========
void mq135_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %d", err);
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = MQ135_WIDTH,
        .atten = MQ135_ATTEN,
    };
    adc_oneshot_config_channel(adc_handle, MQ135_PIN, &chan_cfg);

    // Try to load calibration from NVS
    float ro_nvs = 0.0f;
    if (load_ro_from_nvs(&ro_nvs) == ESP_OK && ro_nvs > 0.0f) {
        calibration_Ro = ro_nvs;
        is_calibrated = true;
        ESP_LOGI(TAG, "✓ MQ135 initialized with saved calibration (Ro=%.2f kΩ)", calibration_Ro);
    } else {
        ESP_LOGW(TAG, "MQ135 initialized WITHOUT calibration - please calibrate!");
    }
}

uint16_t mq135_read_raw(void) {
    int raw = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < 10; i++) {
        int val = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, MQ135_PIN, &val);
        if (err == ESP_OK && val >= 0 && val <= 4095) {
            raw += val;
            valid_samples++;
        } else {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    if (valid_samples == 0) {
        ESP_LOGE(TAG, "All ADC reads failed!");
        return 0;
    }
    
    return (uint16_t)(raw / valid_samples);
}

// Calculate Rs (sensor resistance in kΩ) from ADC value
static float calculate_resistance(uint16_t adc_raw) {
    if (adc_raw == 0 || adc_raw >= 4095) {
        return -1.0f;  // Invalid
    }
    
    // V_sensor = ADC_raw / 4095 * VCC
    float V_sensor = ((float)adc_raw / 4095.0f) * VCC;
    
    // Voltage divider: V_sensor = VCC * RL / (Rs + RL)
    // Solving for Rs: Rs = RL * (VCC / V_sensor - 1)
    float Rs = RL_VALUE * ((VCC / V_sensor) - 1.0f);
    
    if (Rs < 0.1f || Rs > 1000.0f) {
        ESP_LOGW(TAG, "Rs out of range: %.2f kΩ (raw=%u, V=%.3fV)", Rs, adc_raw, V_sensor);
        return -1.0f;
    }
    
    return Rs;
}

float mq135_read_ppm(void) {
    uint16_t raw = mq135_read_raw();
    
    // Check if sensor is connected
    if (raw < 50 || raw > 4000) {
        ESP_LOGD(TAG, "MQ135 sensor not connected or invalid: raw=%u", raw);
        return -1.0f;
    }
    
    float Rs = calculate_resistance(raw);
    if (Rs < 0.0f) {
        ESP_LOGD(TAG, "Invalid sensor resistance");
        return -1.0f;
    }
    
    // If not calibrated, return raw-based estimate
    if (!is_calibrated || calibration_Ro <= 0.0f) {
        ESP_LOGD(TAG, "Not calibrated, using raw estimate");
        // Simple linear mapping: raw 0-4095 → 400-2000 ppm
        return 400.0f + ((float)raw / 4095.0f) * 1600.0f;
    }
    
    // Calculate Rs/Ro
    float Rs_Ro = Rs / calibration_Ro;
    
    // Sanity check
    if (Rs_Ro < 0.01f || Rs_Ro > 100.0f) {
        ESP_LOGW(TAG, "Rs/Ro out of range: %.3f (Rs=%.2f, Ro=%.2f)", Rs_Ro, Rs, calibration_Ro);
        return -1.0f;
    }
    
    // Calculate PPM using power law: ppm = a * (Rs/Ro)^b
    float ppm = PARA_A * powf(Rs_Ro, PARA_B);
    
    if (!isfinite(ppm) || ppm < 0.0f || ppm > 10000.0f) {
        ESP_LOGW(TAG, "PPM calculation invalid: %.2f (Rs/Ro=%.3f)", ppm, Rs_Ro);
        return -1.0f;
    }
    
    ESP_LOGD(TAG, "raw=%u, Rs=%.2fkΩ, Rs/Ro=%.3f, PPM=%.1f", raw, Rs, Rs_Ro, ppm);
    return ppm;
}

uint8_t mq135_get_air_quality_level(void) {
    float ppm = mq135_read_ppm();

    if (ppm >= 0.0f) {
        if (ppm < 600) return 0;      // Good
        else if (ppm < 800) return 1;  // Fair
        else if (ppm < 1000) return 2; // Moderate
        else if (ppm < 1500) return 3; // Poor
        else return 4;                 // Very Poor
    }

    // Fallback: use raw ADC thresholds
    uint16_t raw = mq135_read_raw();
    if (raw <= 819) return 0;
    if (raw <= 1638) return 1;
    if (raw <= 2457) return 2;
    if (raw <= 3276) return 3;
    return 4;
}

esp_err_t mq135_calibrate(void) {
    const int samples = 50;
    uint32_t sum = 0;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Starting MQ135 calibration...");
    ESP_LOGI(TAG, "⚠️  IMPORTANT: Ensure sensor is in CLEAN AIR (outdoor or well-ventilated area)");
    ESP_LOGI(TAG, "========================================");
    
    // Test first reading
    uint16_t first = mq135_read_raw();
    ESP_LOGI(TAG, "First reading: %u", first);
    if (first == 0 || first > 4000) {
        ESP_LOGE(TAG, "Sensor not responding - check connections!");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Collect samples
    for (int i = 0; i < samples; i++) {
        uint16_t r = mq135_read_raw();
        sum += r;
        if (i % 10 == 0) {
            ESP_LOGI(TAG, "Progress: %d/%d, current raw=%u", i, samples, r);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    uint32_t raw_avg = sum / samples;
    ESP_LOGI(TAG, "Average raw value: %lu", raw_avg);
    
    if (raw_avg == 0 || raw_avg > 4095) {
        ESP_LOGE(TAG, "Invalid calibration value: %lu", raw_avg);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate Ro (resistance in clean air = 400ppm CO2)
    float Rs_clean = calculate_resistance(raw_avg);
    if (Rs_clean < 0.0f) {
        ESP_LOGE(TAG, "Failed to calculate resistance from raw=%lu", raw_avg);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ro = Rs / (a * (400^b)) where 400ppm is clean air
    // For simplicity, we assume Rs measured = Ro (sensor in 400ppm baseline)
    calibration_Ro = Rs_clean;
    is_calibrated = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✓ Calibration successful!");
    ESP_LOGI(TAG, "  Raw avg: %lu", raw_avg);
    ESP_LOGI(TAG, "  Ro: %.2f kΩ", calibration_Ro);
    ESP_LOGI(TAG, "========================================");
    
    // Save to NVS
    esp_err_t err = save_ro_to_nvs(calibration_Ro);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✓ Calibration saved to NVS (persistent across reboots)");
    } else {
        ESP_LOGW(TAG, "✗ Failed to save to NVS (will be lost after reboot)");
    }
    
    return ESP_OK;
}

bool mq135_has_calibration(void) {
    return is_calibrated && (calibration_Ro > 0.0f);
}

esp_err_t mq135_clear_calibration(void) {
    calibration_Ro = 0.0f;
    is_calibrated = false;
    
    // Clear from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_KEY_RO);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Calibration cleared from NVS");
    }
    
    ESP_LOGI(TAG, "MQ135 calibration cleared");
    return ESP_OK;
}

void mq135_deinit(void) {
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    calibration_Ro = 0.0f;
    is_calibrated = false;
}