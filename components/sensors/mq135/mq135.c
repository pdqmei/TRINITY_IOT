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
// Load resistance (module MQ-135 thường là 10k hoặc 1k tùy module)
#define RL_VALUE 10.0f          // kOhm - kiểm tra trên module của bạn

// Module MQ-135 có mạch chia áp, output 0-3.3V cho ESP32
// Nếu module chạy 5V heater nhưng có opamp/divider -> output 3.3V max
#define VCC_SENSOR 3.3f  // Điện áp tham chiếu cho ADC (module đã chia áp)

// Datasheet: Rs/Ro ≈ 3.6 trong không khí sạch (~400ppm CO2)
#define CLEAN_AIR_FACTOR 3.6f

// CO2 curve (estimated, from datasheet log-log)
#define PARA_A 116.6020682f
#define PARA_B -2.769034857f

// Số mẫu để lấy trung bình (tăng lên để ổn định hơn)
#define ADC_SAMPLES 50
#define ADC_SAMPLE_DELAY_MS 10

// ================= NVS =================
#define NVS_NAMESPACE "mq135"
#define NVS_KEY_RO "ro_value"

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static float calibration_Ro = 0.0f;   // kOhm
static bool is_calibrated = false;

// ================= NVS FUNCTIONS =================
__attribute__((unused))
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
    int valid_samples = 0;
    int readings[ADC_SAMPLES];
    
    // Đọc nhiều mẫu
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int val = 0;
        if (adc_oneshot_read(adc_handle, MQ135_PIN, &val) == ESP_OK) {
            readings[valid_samples++] = val;
        }
        vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_DELAY_MS));
    }
    
    if (valid_samples < 10) return 0;  // Không đủ mẫu
    
    // Sắp xếp để loại bỏ outliers (bubble sort đơn giản)
    for (int i = 0; i < valid_samples - 1; i++) {
        for (int j = 0; j < valid_samples - i - 1; j++) {
            if (readings[j] > readings[j + 1]) {
                int temp = readings[j];
                readings[j] = readings[j + 1];
                readings[j + 1] = temp;
            }
        }
    }
    
    // Lấy trung bình 60% ở giữa (loại bỏ 20% cao nhất và 20% thấp nhất)
    int start = valid_samples / 5;          // 20%
    int end = valid_samples - start;        // 80%
    
    for (int i = start; i < end; i++) {
        sum += readings[i];
    }
    
    return (uint16_t)(sum / (end - start));
}

// ================= ĐƠN GIẢN HÓA: MAPPING TRỰC TIẾP TỪ ADC RAW =================
// Không dùng công thức phức tạp vì module MQ135 rẻ tiền không chính xác
// Thay vào đó, dùng mapping tuyến tính từ ADC raw sang PPM ước lượng
//
// ADC Range (đã test thực tế):
//   200-400:   Không khí sạch (~400 PPM)
//   400-600:   Bình thường (~500-800 PPM)
//   600-900:   Hơi ô nhiễm (~800-1200 PPM)
//   900-1300:  Ô nhiễm (~1200-2000 PPM)
//   1300-1800: Nặng (~2000-3000 PPM)
//   >1800:     Rất nặng (>3000 PPM)

// Lưu giá trị PPM trước để làm mượt
static float last_valid_ppm = 400.0f;

float mq135_read_ppm(void) {
    uint16_t raw = mq135_read_raw();
    
    if (raw < 50 || raw > 4000) {
        ESP_LOGW(TAG, "ADC raw invalid: %u", raw);
        return last_valid_ppm;
    }
    
    // Mapping đơn giản từ ADC raw sang PPM ước lượng
    // Công thức tuyến tính: ppm = base + (raw - min) * scale
    float ppm;
    
    if (raw < 300) {
        // Không khí rất sạch: 350-450 PPM
        ppm = 350.0f + (raw - 100) * 0.5f;
    } 
    else if (raw < 600) {
        // Không khí sạch: 400-700 PPM
        ppm = 400.0f + (raw - 300) * 1.0f;
    }
    else if (raw < 900) {
        // Bình thường: 700-1000 PPM
        ppm = 700.0f + (raw - 600) * 1.0f;
    }
    else if (raw < 1300) {
        // Hơi ô nhiễm: 1000-1500 PPM
        ppm = 1000.0f + (raw - 900) * 1.25f;
    }
    else if (raw < 1800) {
        // Ô nhiễm: 1500-2500 PPM
        ppm = 1500.0f + (raw - 1300) * 2.0f;
    }
    else {
        // Rất ô nhiễm: 2500+ PPM
        ppm = 2500.0f + (raw - 1800) * 2.5f;
    }
    
    // Giới hạn PPM hợp lý
    if (ppm < 350.0f) ppm = 350.0f;
    if (ppm > 5000.0f) ppm = 5000.0f;
    
    // Exponential Moving Average để làm mượt (alpha = 0.3)
    float smoothed_ppm = 0.3f * ppm + 0.7f * last_valid_ppm;
    last_valid_ppm = smoothed_ppm;

    ESP_LOGI(TAG, "ADC=%u → PPM=%.0f (smooth=%.0f)", raw, ppm, smoothed_ppm);

    return smoothed_ppm;
}

// ================= CALIBRATION (không cần thiết với mapping đơn giản) =================
esp_err_t mq135_calibrate(void) {
    ESP_LOGI(TAG, "MQ135 using simple ADC mapping - no calibration needed");
    
    // Đọc vài mẫu để warmup ADC
    for (int i = 0; i < 10; i++) {
        mq135_read_raw();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    is_calibrated = true;
    calibration_Ro = 10.0f;  // Giá trị dummy
    
    ESP_LOGI(TAG, "MQ135 warmup done, ready to use");
    return ESP_OK;
}

bool mq135_has_calibration(void) {
    // Với mapping đơn giản, luôn sẵn sàng sau khi init
    return true;
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
