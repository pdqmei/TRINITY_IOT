#include "mq135.h"
#include "esp_log.h"
#include <math.h>

#define MQ135_PIN ADC_CHANNEL_0
#define MQ135_ATTEN ADC_ATTEN_DB_11
#define MQ135_WIDTH ADC_BITWIDTH_12

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;

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

    // MQ135 PPM calculation (simplified)
    float Rs_Ro = voltage / (3.3f - voltage + 1e-6f);
    float ppm = 116.6f * powf(Rs_Ro, -2.769f);

    ESP_LOGI(TAG, "MQ135 PPM: %.2f", ppm);

    return ppm;
}

uint8_t mq135_get_air_quality_level(void) {
    float ppm = mq135_read_ppm();

    if (ppm < 400) return 0;      // Good
    else if (ppm < 600) return 1; // Fair
    else if (ppm < 800) return 2; // Moderate
    else if (ppm < 1000) return 3; // Poor
    else return 4;                 // Very Poor
}

void mq135_deinit(void) {
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
}
