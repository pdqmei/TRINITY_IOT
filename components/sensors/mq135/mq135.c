#include "mq135.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
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

// Số mẫu để lấy trung bình
#define ADC_SAMPLES 20
#define ADC_SAMPLE_DELAY_MS 5

// ================= NVS =================
#define NVS_NAMESPACE "mq135"
#define NVS_KEY_RO "ro_value"

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;  // ⭐ THÊM: ADC calibration handle
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

    // ⭐ THÊM: Khởi tạo ADC calibration để đọc chính xác hơn
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = MQ135_ATTEN,
        .bitwidth = MQ135_WIDTH,
    };
    
    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized successfully");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values");
        adc_cali_handle = NULL;
    }

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
    
    if (valid_samples < 5) return 0;  // Không đủ mẫu
    
    // Lấy trung bình đơn giản - KHÔNG loại outliers để phản hồi nhanh
    for (int i = 0; i < valid_samples; i++) {
        sum += readings[i];
    }
    
    return (uint16_t)(sum / valid_samples);
}

// ================= TÍNH PPM THEO CÔNG THỨC DATASHEET =================
// Công thức: PPM = A * (Rs/Ro)^B
// Trong đó:
//   Rs = Điện trở sensor hiện tại (tính từ ADC)
//   Ro = Điện trở trong không khí sạch (từ calibration)
//   A = 116.6020682, B = -2.769034857 (hệ số cho CO2)
//
// Tính Rs từ mạch chia áp:
//   Vout = ADC_raw / 4095 * Vref
//   Rs = RL * (Vcc - Vout) / Vout

// Lưu giá trị PPM trước để làm mượt
static float last_valid_ppm = 400.0f;
static bool sensor_connected = false;

// Kiểm tra sensor có kết nối không
// Sensor MQ135 khi hoạt động thường cho ADC trong khoảng 100-3500
// Nếu ADC quá thấp (<30) hoặc quá cao (>4050) liên tục -> không có sensor
bool mq135_is_connected(void) {
    int stable_count = 0;
    int invalid_count = 0;
    
    for (int i = 0; i < 10; i++) {
        int val = 0;
        if (adc_oneshot_read(adc_handle, MQ135_PIN, &val) == ESP_OK) {
            // Giá trị hợp lệ khi sensor kết nối: 100-3800
            if (val >= 100 && val <= 3800) {
                stable_count++;
            }
            // Giá trị bất thường (floating hoặc short)
            else if (val < 30 || val > 4050) {
                invalid_count++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Sensor kết nối nếu có ít nhất 6/10 mẫu hợp lệ
    sensor_connected = (stable_count >= 6);
    
    if (!sensor_connected) {
        ESP_LOGW(TAG, "MQ135 sensor NOT connected! (valid=%d, invalid=%d)", 
                 stable_count, invalid_count);
    }
    
    return sensor_connected;
}

float mq135_read_ppm(void) {
    uint16_t raw = mq135_read_raw();
    
    // Kiểm tra giá trị ADC hợp lệ
    // Floating pin thường cho giá trị rất thấp (<30) hoặc rất cao (>4050)
    // hoặc dao động mạnh
    if (raw < 100 || raw > 3800) {
        ESP_LOGW(TAG, "ADC raw invalid: %u (sensor may not be connected)", raw);
        sensor_connected = false;
        return -1.0f;  // Trả về -1 để báo lỗi
    }
    
    sensor_connected = true;
    
    // ⭐ CẢI TIẾN: Dùng ADC calibration thay vì linear mapping
    // Tính Vout từ ADC (12-bit = 4095, Vref = 3.3V)
    float vout;
    if (adc_cali_handle) {
        // Sử dụng calibration chính xác
        int voltage_mv = 0;
        esp_err_t ret = adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage_mv);
        if (ret == ESP_OK) {
            vout = voltage_mv / 1000.0f;  // Convert mV to V
        } else {
            // Fallback nếu calibration thất bại
            vout = (raw / 4095.0f) * VCC_SENSOR;
        }
    } else {
        // Không có calibration, dùng linear mapping
        vout = (raw / 4095.0f) * VCC_SENSOR;
    }
    
    // Tránh chia cho 0 và giới hạn voltage range
    if (vout < 0.05f) {
        vout = 0.05f;
    }
    if (vout > 3.25f) {
        vout = 3.25f;
    }
    
    // Tính Rs (kOhm) từ mạch chia áp
    // Rs = RL * (Vcc - Vout) / Vout
    float rs = RL_VALUE * (VCC_SENSOR - vout) / vout;
    
    // Sử dụng Ro từ calibration, nếu chưa có thì dùng giá trị mặc định
    float ro = calibration_Ro;
    if (ro <= 0.0f) {
        // Ro mặc định = Rs_clean_air / CLEAN_AIR_FACTOR
        // Giả sử Rs trong không khí sạch khoảng 30-50 kOhm
        ro = 10.0f;  // kOhm - giá trị mặc định hợp lý
    }
    
    // Tính tỷ lệ Rs/Ro
    float ratio = rs / ro;
    
    // ⭐ CẢI TIẾN: Giới hạn ratio chặt chẽ hơn để tránh giá trị cực đoan
    if (ratio < 0.5f) ratio = 0.5f;   // Thay vì 0.1
    if (ratio > 8.0f) ratio = 8.0f;   // Thay vì 10.0
    
    // Công thức PPM = A * (Rs/Ro)^B
    // PPM = 116.6 * ratio^(-2.769)
    float ppm = PARA_A * powf(ratio, PARA_B);
    
    // Giới hạn PPM trong khoảng hợp lý
    if (ppm < 350.0f) ppm = 350.0f;
    if (ppm > 9999.0f) ppm = 9999.0f;
    
    // ⭐ CẢI TIẾN: Giảm alpha để ổn định hơn (0.3 thay vì 0.6)
    // Exponential Moving Average - alpha = 0.3 để ưu tiên ổn định hơn phản hồi
    float smoothed_ppm = 0.3f * ppm + 0.7f * last_valid_ppm;
    last_valid_ppm = smoothed_ppm;

    ESP_LOGI(TAG, "ADC=%u, Vout=%.3fV, Rs=%.2fkΩ, Rs/Ro=%.2f → PPM=%.0f (smooth=%.0f)", 
             raw, vout, rs, ratio, ppm, smoothed_ppm);

    return smoothed_ppm;
}

// ================= CALIBRATION =================
esp_err_t mq135_calibrate(void) {
    ESP_LOGI(TAG, "Starting MQ135 calibration in clean air...");
    ESP_LOGI(TAG, "Please ensure sensor is in clean outdoor air (~400ppm CO2)");
    
    // ⭐ CẢI TIẾN: Warmup đầy đủ hơn
    // Đọc vài mẫu để warmup ADC
    for (int i = 0; i < 10; i++) {
        mq135_read_raw();
        vTaskDelay(pdMS_TO_TICKS(1000));
        if ((i + 1) % 2 == 0) {
            ESP_LOGI(TAG, "Warmup: %d/10 seconds", i + 1);
        }
    }
    
    // ⭐ CẢI TIẾN: Thu thập samples để tính Ro thực sự
    ESP_LOGI(TAG, "Collecting calibration samples...");
    float rs_sum = 0.0f;
    int valid_samples = 0;
    
    for (int i = 0; i < 50; i++) {
        uint16_t raw = mq135_read_raw();
        
        if (raw < 100 || raw > 3800) {
            ESP_LOGW(TAG, "Invalid sample %d: ADC=%u", i, raw);
            continue;
        }
        
        // Convert to voltage using calibration if available
        float vout;
        if (adc_cali_handle) {
            int voltage_mv = 0;
            adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage_mv);
            vout = voltage_mv / 1000.0f;
        } else {
            vout = (raw / 4095.0f) * VCC_SENSOR;
        }
        
        if (vout < 0.05f) vout = 0.05f;
        
        // Calculate Rs
        float rs = RL_VALUE * (VCC_SENSOR - vout) / vout;
        rs_sum += rs;
        valid_samples++;
        
        if (i % 10 == 0) {
            ESP_LOGI(TAG, "Sample %d: ADC=%u, Rs=%.2fkΩ", i, raw, rs);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (valid_samples < 30) {
        ESP_LOGE(TAG, "Calibration failed: not enough valid samples (%d)", valid_samples);
        // Fallback to default
        calibration_Ro = 10.0f;
        is_calibrated = true;
        return ESP_FAIL;
    }
    
    // Calculate average Rs and Ro
    float rs_avg = rs_sum / valid_samples;
    calibration_Ro = rs_avg / CLEAN_AIR_FACTOR;
    
    ESP_LOGI(TAG, "Calibration complete:");
    ESP_LOGI(TAG, "  Valid samples: %d", valid_samples);
    ESP_LOGI(TAG, "  Average Rs: %.2f kΩ", rs_avg);
    ESP_LOGI(TAG, "  Calculated Ro: %.2f kΩ", calibration_Ro);
    
    // Save to NVS
    esp_err_t err = save_ro_to_nvs(calibration_Ro);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Calibration saved to NVS");
        is_calibrated = true;
    } else {
        ESP_LOGW(TAG, "Failed to save calibration to NVS");
        is_calibrated = true;  // Still mark as calibrated
    }
    
    ESP_LOGI(TAG, "MQ135 calibration ready");
    return ESP_OK;
}

bool mq135_has_calibration(void) {
    // ⭐ CẢI TIẾN: Kiểm tra thực sự có calibration
    return is_calibrated && (calibration_Ro > 0.0f);
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
    // ⭐ THÊM: Cleanup ADC calibration handle
    if (adc_cali_handle) {
        adc_cali_delete_scheme_line_fitting(adc_cali_handle);
        adc_cali_handle = NULL;
    }
    
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    is_calibrated = false;
    calibration_Ro = 0.0f;
}