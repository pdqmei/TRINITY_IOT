#include "mq135.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "MQ135";

// Đổi từ ADC1_CHANNEL_6 thành ADC_CHANNEL_6
#define MQ135_ADC_CHANNEL  ADC_CHANNEL_6
#define MQ135_ADC_UNIT     ADC_UNIT_1

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;

void mq135_init(void) {
    // Khởi tạo ADC oneshot
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = MQ135_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    // Cấu hình channel - Đổi ADC_ATTEN_DB_11 thành ADC_ATTEN_DB_12
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MQ135_ADC_CHANNEL, &config));

    adc_cali_line_fitting_config_t cal_cfg = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
    };
    
    // Đổi từ adc_cali_create_scheme_curve_fitting
    if (adc_cali_create_scheme_line_fitting(&cal_cfg, &adc1_cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    }
}

int mq135_read_raw(void) {
    int raw = 0;
    adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &raw);
    return raw;
}

int mq135_read_voltage(void) {
    int raw = mq135_read_raw();
    int voltage = 0;
    
    if (adc1_cali_handle != NULL) {
        adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage);
    }
    
    return voltage;
}