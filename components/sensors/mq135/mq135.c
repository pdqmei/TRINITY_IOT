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

// ================= MQ135 CONSTANTS =================
// Load resistance (module MQ-135 thường là 10k)
#define RL_VALUE 10.0f          // kOhm

// ⚠️ MQ-135 chạy ở 5V (heater + mạch analog)
#define VCC 5.0f

// Datasheet: Rs/Ro ≈ 3.6 trong không khí sạch (~400ppm CO2)
#define CLEAN_AIR_FACTOR 3.6f

// CO2 curve (estimated, from datasheet log-log)
#define PARA_A 116.6020682f
#define PARA_B -2.769034857f

// ================= NVS =================
#define NVS_NAMESPACE "mq135"
#define NVS_KEY_RO "ro_value"

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static float calibration_Ro = 0.0f;   // kOhm
static bool is_calibrated = false;

// ================= NVS FUNCTIONS =================
static esp_err_t save_ro_to_nvs(float ro_value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs_handle, NVS_KEY_RO, &ro_value, sizeof(float));
    if (err == ESP_OK) err = nvs_commit(nvs_handle);

    nvs_close(nvs_handle);
    return err;
}

static esp_err_t load_ro_from_nvs(float *ro_value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(float);
    err = nvs_get_blob(nvs_handle, NVS_KEY_RO, ro_value, &size);
    nvs_close(nvs_handle);
    return err;
}

// ================= ADC =================
void mq135_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = MQ135_WIDTH,
        .atten = MQ135_ATTEN,
    };
    adc_oneshot_config_channel(adc_handle, MQ135_PIN, &chan_cfg);

    float ro_nvs = 0.0f;
    if (load_ro_from_nvs(&ro_nvs) == ESP_OK && ro_nvs > 0.0f) {
        calibration_Ro = ro_nvs;
        is_calibrated = true;
        ESP_LOGI(TAG, "Loaded calibration Ro = %.2f kOhm", calibration_Ro);
    } else {
        ESP_LOGW(TAG, "No calibration found – please calibrate!");
    }
}

uint16_t mq135_read_raw(void) {
    uint32_t sum = 0;
    int samples = 20;

    for (int i = 0; i < samples; i++) {
        int val = 0;
        if (adc_oneshot_read(adc_handle, MQ135_PIN, &val) == ESP_OK) {
            sum += val;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return (uint16_t)(sum / samples);
}

// ================= CORE CALC =================
static float calculate_resistance(uint16_t adc_raw) {
    if (adc_raw == 0 || adc_raw >= 4095) return -1.0f;

    // ADC đo 0–3.3V, module MQ-135 chia áp từ 5V
    float Vout = ((float)adc_raw / 4095.0f) * 3.3f;

    // Rs = RL * (Vc / Vout - 1)
    float Rs = RL_VALUE * ((VCC / Vout) - 1.0f);
    return (Rs > 0.0f) ? Rs : -1.0f;
}

float mq135_read_ppm(void) {
    if (!is_calibrated || calibration_Ro <= 0.0f) {
        ESP_LOGW(TAG, "MQ135 not calibrated – ppm unavailable");
        return -1.0f;
    }

    uint16_t raw = mq135_read_raw();
    float Rs = calculate_resistance(raw);
    if (Rs < 0.0f) return -1.0f;

    float ratio = Rs / calibration_Ro;

    // realistic range
    if (ratio < 0.2f || ratio > 10.0f) return -1.0f;

    float ppm = PARA_A * powf(ratio, PARA_B);
    if (!isfinite(ppm) || ppm < 0.0f) return -1.0f;

    ESP_LOGD(TAG, "raw=%u Rs=%.2fk Ro=%.2fk Rs/Ro=%.2f ppm=%.1f",
             raw, Rs, calibration_Ro, ratio, ppm);

    return ppm;
}

// ================= CALIBRATION =================
esp_err_t mq135_calibrate(void) {
    ESP_LOGI(TAG, "Calibrating MQ135 – ensure CLEAN AIR!");
    vTaskDelay(pdMS_TO_TICKS(5000));   // warm-up wait

    uint32_t sum = 0;
    int samples = 50;

    for (int i = 0; i < samples; i++) {
        sum += mq135_read_raw();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    uint16_t raw_avg = sum / samples;
    float Rs_clean = calculate_resistance(raw_avg);
    if (Rs_clean < 0.0f) return ESP_FAIL;

    // Datasheet: Rs/Ro ≈ 3.6
    calibration_Ro = Rs_clean / CLEAN_AIR_FACTOR;
    is_calibrated = true;

    save_ro_to_nvs(calibration_Ro);

    ESP_LOGI(TAG, "Calibration done: Ro = %.2f kOhm", calibration_Ro);
    return ESP_OK;
}

bool mq135_has_calibration(void) {
    return is_calibrated;
}

esp_err_t mq135_clear_calibration(void) {
    calibration_Ro = 0.0f;
    is_calibrated = false;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_erase_key(nvs, NVS_KEY_RO);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    return ESP_OK;
}

void mq135_deinit(void) {
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    is_calibrated = false;
    calibration_Ro = 0.0f;
}
