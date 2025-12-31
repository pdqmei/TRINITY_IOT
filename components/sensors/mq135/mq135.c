#include "mq135.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define MQ135_PIN ADC_CHANNEL_0
#define MQ135_ATTEN ADC_ATTEN_DB_12
#define MQ135_WIDTH ADC_BITWIDTH_12

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static float calibration_vro = 0.0f;
static bool is_calibrated = false;

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

    ESP_LOGI(TAG, "MQ135 initialized (oneshot ADC)");
}

uint16_t mq135_read_raw(void) {
    int raw = 0;
    for (int i = 0; i < 10; i++) {
        int val = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, MQ135_PIN, &val);
        if (err == ESP_OK) raw += val;
        else ESP_LOGW(TAG, "adc read failed: %d", err);
    }
    return (uint16_t)(raw / 10);
}

float mq135_read_ppm(void) {
    uint16_t raw = mq135_read_raw();
    float voltage = (raw / 4095.0f) * 3.3f;

    if (!is_calibrated || calibration_vro <= 0.0f) {
        ESP_LOGW(TAG, "MQ135 not calibrated, skipping accurate PPM computation");
        return -1.0f;
    }

    float V = voltage;
    float Vro = calibration_vro;
    float denom = (V * (3.3f - Vro));
    if (denom <= 0.0f) {
        ESP_LOGW(TAG, "MQ135 invalid voltage for PPM calculation V=%.3f Vro=%.3f", V, Vro);
        return -1.0f;
    }
    float Rs_Ro = ((3.3f - V) * Vro) / denom;
    if (Rs_Ro <= 0.0f) {
        ESP_LOGW(TAG, "MQ135 computed non-positive Rs/Ro=%.6f", Rs_Ro);
        return -1.0f;
    }

    float ppm = 116.6f * powf(Rs_Ro, -2.769f);
    if (!isfinite(ppm) || ppm < 0.0f) {
        ESP_LOGW(TAG, "MQ135 PPM computation produced invalid value: %f", ppm);
        return -1.0f;
    }

    ESP_LOGI(TAG, "MQ135 PPM: %.2f", ppm);
    return ppm;
}

uint8_t mq135_get_air_quality_level(void) {
    float ppm = mq135_read_ppm();

    if (ppm >= 0.0f) {
        if (ppm < 400) return 0;
        else if (ppm < 600) return 1;
        else if (ppm < 800) return 2;
        else if (ppm < 1000) return 3;
        else return 4;
    }

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
    
    ESP_LOGI(TAG, "Starting MQ135 calibration...");
    
    for (int i = 0; i < samples; i++) {
        uint16_t r = mq135_read_raw();
        sum += r;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    uint32_t raw_ro = sum / samples;
    if (raw_ro == 0 || raw_ro > 4095) {
        ESP_LOGE(TAG, "Invalid calibration value: %lu", raw_ro);
        return ESP_ERR_INVALID_ARG;
    }

    calibration_vro = ((float)raw_ro / 4095.0f) * 3.3f;
    is_calibrated = true;
    
    ESP_LOGI(TAG, "MQ135 calibrated: raw_ro=%lu Vro=%.3f", raw_ro, calibration_vro);
    ESP_LOGW(TAG, "Calibration stored in RAM - will be lost after reboot");
    
    return ESP_OK;
}

bool mq135_has_calibration(void) {
    return is_calibrated && (calibration_vro > 0.0f);
}

esp_err_t mq135_clear_calibration(void) {
    calibration_vro = 0.0f;
    is_calibrated = false;
    ESP_LOGI(TAG, "MQ135 calibration cleared");
    return ESP_OK;
}

void mq135_deinit(void) {
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    calibration_vro = 0.0f;
    is_calibrated = false;
}