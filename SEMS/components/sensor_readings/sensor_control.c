#include "sensor_control.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "SENSOR";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibrated = false;


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

// ===== ĐỌC DỮ LIỆU TỪ MQ-135 (ADC RAW) =====
int read_mq135_raw(void)
{
    int adc_raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(
        adc1_handle,
        MQ135_ADC_CHANNEL,
        &adc_raw
    ));

    ESP_LOGI(TAG, "💨 MQ-135 ADC Raw: %d", adc_raw);
    return adc_raw;
}

static float read_mq135_ppm_from_raw(int adc_raw)
{
    int voltage_mv = 0;

    if (adc_calibrated) {
        adc_cali_raw_to_voltage(
            adc1_cali_handle,
            adc_raw,
            &voltage_mv
        );
    } else {
        voltage_mv = (adc_raw * 3300) / 4095;
    }

    float voltage = voltage_mv / 1000.0;

    float RL = 10000.0;  // 10kΩ
    float Vc = 3.3;

    if (voltage < 0.001f) voltage = 0.001f;

    float Rs = (Vc - voltage) * RL / voltage;

    float R0 = 10000.0;   // giá trị giả định
    float ratio = Rs / R0;

    float ppm = 100.0f * powf(ratio, -1.5f);

    ESP_LOGI(TAG, "💨 MQ-135 CO2 estimated: %.2f ppm", ppm);

    return ppm;
}

float read_mq135_ppm(void)
{
    int raw = read_mq135_raw();
    return read_mq135_ppm_from_raw(raw);
}

// ===== KHỞI TẠO TẤT CẢ SENSORS =====
void sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors...");
    
    // 1. Khởi tạo I2C cho SHT31
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed!");
    }
    
    // 2. Khởi tạo ADC cho MQ-135
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc1_handle,
        MQ135_ADC_CHANNEL,
        &chan_config
    ));

    // ===== ADC CALIBRATION =====
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = MQ135_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(
            &cali_config,
            &adc1_cali_handle) == ESP_OK) {
        adc_calibrated = true;
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "ADC calibration not supported");
    }

    
    ESP_LOGI(TAG, "✅ Sensors initialized successfully");
    
    // Test đọc ngay 1 lần
    ESP_LOGI(TAG, "Testing sensors...");
    float temp, humi;
    if (read_sht31(&temp, &humi)) {
        ESP_LOGI(TAG, "✅ SHT31 working!");
    } else {
        ESP_LOGW(TAG, "⚠️  SHT31 test failed - check I2C connection");
    }
    
    int mq135_raw = read_mq135_raw();
    ESP_LOGI(TAG, "✅ MQ-135 ADC: %d", mq135_raw);
}

// ===== ĐỌC TẤT CẢ SENSORS =====
sensor_data_t read_all_sensors(void)
{
    sensor_data_t data;
    
    // Đọc SHT31
    data.is_valid = read_sht31(&data.temperature, &data.humidity);
    
    // Đọc MQ-135
    int raw = read_mq135_raw();
    data.co2_level = raw;
    data.co2_ppm = read_mq135_ppm_from_raw(raw);
    return data;
}
