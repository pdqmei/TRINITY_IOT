#include "sht31.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"

#define SHT31_ADDR 0x44
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO PIN_SHT31_SDA
#define I2C_MASTER_SCL_IO PIN_SHT31_SCL

// Single-shot high repeatability measurement (no clock stretching)
static const uint8_t SHT31_CMD_MEAS_HIGHREP[2] = { 0x2C, 0x06 };

static const char *TAG = "SHT31";

void sht31_init(void) {
    // Check if I2C driver already installed
    esp_err_t ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    
    if (ret == ESP_ERR_INVALID_STATE) {
        // I2C already initialized (probably by another component)
        ESP_LOGI(TAG, "I2C bus already initialized, skipping driver install");
    } else if (ret == ESP_OK) {
        // First time initialization
        ESP_LOGI(TAG, "Initializing I2C bus (SDA=%d, SCL=%d, 100kHz)", 
                 I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
        
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,  // 100kHz for compatibility with LCD
        };
        i2c_param_config(I2C_MASTER_NUM, &conf);
        
        // Note: driver already installed above, just configure parameters
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize I2C: %d", ret);
    }
}

sht31_data_t sht31_read(void) {
    sht31_data_t data = {0.0f, 0.0f};
    uint8_t buffer[6] = {0};

    // Send measurement command
    i2c_master_write_to_device(I2C_MASTER_NUM, SHT31_ADDR, SHT31_CMD_MEAS_HIGHREP, sizeof(SHT31_CMD_MEAS_HIGHREP), pdMS_TO_TICKS(100));
    // Wait for measurement to complete (per datasheet ~15ms)
    vTaskDelay(pdMS_TO_TICKS(20));

    // Read 6 bytes: temp msb, temp lsb, temp CRC, hum msb, hum lsb, hum CRC
    esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, SHT31_ADDR, buffer, 6, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %d", err);
        return data;
    }

    uint16_t temp_raw = (buffer[0] << 8) | buffer[1];
    uint16_t hum_raw = (buffer[3] << 8) | buffer[4];

    data.temperature = -45.0f + (175.0f * ((float)temp_raw / 65535.0f));
    data.humidity = 100.0f * ((float)hum_raw / 65535.0f);

    ESP_LOGI(TAG, "Temp: %.2f°C, Humidity: %.2f%%", data.temperature, data.humidity);

    return data;
}

float sht31_get_temperature(void) {
    return sht31_read().temperature;
}

float sht31_get_humidity(void) {
    return sht31_read().humidity;
}
