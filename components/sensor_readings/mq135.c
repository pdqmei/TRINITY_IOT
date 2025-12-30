#include "mq135.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include <math.h>

#define MQ135_PIN ADC1_CHANNEL_0
#define MQ135_ATTEN ADC_ATTEN_DB_11
#define MQ135_WIDTH ADC_WIDTH_BIT_12

static const char *TAG = "MQ135";

void mq135_init(void) {
    adc1_config_width(MQ135_WIDTH);
    adc1_config_channel_atten(MQ135_PIN, MQ135_ATTEN);
    ESP_LOGI(TAG, "MQ135 initialized");
}

uint16_t mq135_read_raw(void) {
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += adc1_get_raw(MQ135_PIN);
    }
    return sum / 10;
}

float mq135_read_ppm(void) {
    uint16_t raw = mq135_read_raw();
    float voltage = (raw / 4095.0) * 3.3;
    
    float Rs_Ro = voltage / (3.3 - voltage);
    float ppm = 116.6 * pow(Rs_Ro, -2.769);
    
    ESP_LOGI(TAG, "MQ135 PPM: %.2f", ppm);
    
    return ppm;
}

uint8_t mq135_get_air_quality_level(void) {
    float ppm = mq135_read_ppm();
    
    if (ppm < 400) return 0;
    else if (ppm < 600) return 1;
    else if (ppm < 800) return 2;
    else if (ppm < 1000) return 3;
    else return 4;
}
