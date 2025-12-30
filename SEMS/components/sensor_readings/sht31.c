#include "sht31.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* ===== CONFIG ===== */
#define TAG "SHT31"

#define SHT31_ADDR          0x44
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_SDA             GPIO_NUM_21
#define I2C_SCL             GPIO_NUM_22
#define I2C_FREQ            100000

bool sht31_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_FREQ,
        },
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

bool sht31_read(float *temperature, float *humidity)
{
    uint8_t cmd[2] = {0x2C, 0x06};
    uint8_t rx[6];

    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (SHT31_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(handle, cmd, 2, true);
    i2c_master_stop(handle);

    if (i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(1000)) != ESP_OK) {
        i2c_cmd_link_delete(handle);
        return false;
    }
    i2c_cmd_link_delete(handle);

    vTaskDelay(pdMS_TO_TICKS(20));

    handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (SHT31_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(handle, rx, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(handle);

    if (i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(1000)) != ESP_OK) {
        i2c_cmd_link_delete(handle);
        return false;
    }
    i2c_cmd_link_delete(handle);

    uint16_t rawT = (rx[0] << 8) | rx[1];
    uint16_t rawH = (rx[3] << 8) | rx[4];

    *temperature = -45.0f + 175.0f * (rawT / 65535.0f);
    *humidity    = 100.0f * (rawH / 65535.0f);

    return true;
}
