#include "sht31.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define SHT31_ADDR 0x44
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_SCL_IO GPIO_NUM_22

static const char *TAG = "SHT31";

void sht31_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

sht31_data_t sht31_read(void) {
    sht31_data_t data = {0.0, 0.0};
    uint8_t buffer[6];
    
    i2c_master_read_from_device(I2C_MASTER_NUM, SHT31_ADDR, buffer, 6, -1);
    
    uint16_t temp_raw = (buffer[0] << 8) | buffer[1];
    uint16_t hum_raw = (buffer[3] << 8) | buffer[4];
    
    data.temperature = -45 + (175 * temp_raw / 65535.0);
    data.humidity = 100 * hum_raw / 65535.0;
    
    ESP_LOGI(TAG, "Temp: %.2f°C, Humidity: %.2f%%", data.temperature, data.humidity);
    
    return data;
}

float sht31_get_temperature(void) {
    return sht31_read().temperature;
}

float sht31_get_humidity(void) {
    return sht31_read().humidity;
}
