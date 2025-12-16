#include "sensor_control.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "SENSOR";

// ===== KHỞI TẠO I2C CHO SHT31 =====
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed");
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return err;
    }
    
    ESP_LOGI(TAG, "I2C initialized successfully");
    return ESP_OK;
}

// ===== ĐỌC DỮ LIỆU TỪ SHT31 =====
bool read_sht31(float *temp, float *humi)
{
    uint8_t cmd[2] = {0x2C, 0x06};  // Command: High repeatability measurement
    uint8_t data[6];
    
    // Gửi lệnh đo
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT31_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, 2, true);
    i2c_master_stop(cmd_handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 write command failed");
        return false;
    }
    
    // Chờ đo (15ms cho high repeatability)
    vTaskDelay(20 / portTICK_PERIOD_MS);
    
    // Đọc dữ liệu
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT31_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 read data failed");
        return false;
    }
    
    // Tính toán nhiệt độ và độ ẩm
    uint16_t temp_raw = (data[0] << 8) | data[1];
    uint16_t humi_raw = (data[3] << 8) | data[4];
    
    *temp = -45.0 + (175.0 * temp_raw / 65535.0);
    *humi = 100.0 * humi_raw / 65535.0;
    
    // Kiểm tra checksum (optional - data[2] và data[5])
    
    ESP_LOGI(TAG, "🌡️  Temperature: %.2f°C", *temp);
    ESP_LOGI(TAG, "💧 Humidity: %.2f%%", *humi);
    
    return true;
}

// ===== ĐỌC DỮ LIỆU TỪ MQ-9 (ADC RAW) =====
int read_mq9_raw(void)
{
    int adc_value = adc1_get_raw(MQ9_ADC_CHANNEL);
    ESP_LOGI(TAG, "💨 MQ-9 ADC Raw: %d", adc_value);
    return adc_value;
}

// ===== CHUYỂN ĐỔI MQ-9 SANG PPM (ƯỚC LƯỢNG) =====
float read_mq9_ppm(void)
{
    int adc_raw = read_mq9_raw();
    
    // Công thức chuyển đổi (cần hiệu chỉnh theo datasheet MQ-9)
    // Đây là công thức ước lượng, bạn cần calibrate với khí chuẩn
    
    // Voltage = ADC * (3.3V / 4095)
    float voltage = (adc_raw / 4095.0) * 3.3;
    
    // Rs = (Vc - Vout) * RL / Vout
    // Giả sử RL = 10kΩ, Vc = 5V (nếu dùng 5V) hoặc 3.3V
    float RL = 10000.0;  // 10kΩ
    float Vc = 3.3;      // Điện áp nguồn
    
    if (voltage == 0) voltage = 0.001;  // Tránh chia cho 0
    
    float Rs = (Vc - voltage) * RL / voltage;
    
    // Rs/R0 ratio (R0 cần đo trong không khí sạch, giả sử R0 = 10kΩ)
    float R0 = 10000.0;
    float ratio = Rs / R0;
    
    // Công thức từ datasheet MQ-9 (đường cong log-log)
    // ppm = A * ratio^B (cần tra datasheet để lấy A, B chính xác)
    // Ví dụ với CO: ppm = 100 * ratio^(-1.5)
    float ppm = 100.0 * pow(ratio, -1.5);
    
    ESP_LOGI(TAG, "💨 MQ-9 CO2 estimated: %.2f ppm", ppm);
    
    return ppm;
}

// ===== KHỞI TẠO TẤT CẢ SENSORS =====
void sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors...");
    
    // 1. Khởi tạo I2C cho SHT31
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed!");
    }
    
    // 2. Khởi tạo ADC cho MQ-9
    adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0-4095)
    adc1_config_channel_atten(MQ9_ADC_CHANNEL, ADC_ATTEN_DB_11);  // 0-3.3V
    
    ESP_LOGI(TAG, "✅ Sensors initialized successfully");
    
    // Test đọc ngay 1 lần
    ESP_LOGI(TAG, "Testing sensors...");
    float temp, humi;
    if (read_sht31(&temp, &humi)) {
        ESP_LOGI(TAG, "✅ SHT31 working!");
    } else {
        ESP_LOGW(TAG, "⚠️  SHT31 test failed - check I2C connection");
    }
    
    int mq9_raw = read_mq9_raw();
    ESP_LOGI(TAG, "✅ MQ-9 ADC: %d", mq9_raw);
}

// ===== ĐỌC TẤT CẢ SENSORS =====
sensor_data_t read_all_sensors(void)
{
    sensor_data_t data;
    
    // Đọc SHT31
    data.is_valid = read_sht31(&data.temperature, &data.humidity);
    
    // Đọc MQ-9
    data.co2_level = read_mq9_raw();
    data.co2_ppm = read_mq9_ppm();
    
    return data;
}