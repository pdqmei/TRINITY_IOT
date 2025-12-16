#ifndef SENSOR_CONTROL_H
#define SENSOR_CONTROL_H

#include <stdbool.h>

// Địa chỉ I2C của SHT31
#define SHT31_I2C_ADDR      0x44    // Mặc định (có thể là 0x45)

// Cấu hình I2C
#define I2C_MASTER_SCL_IO   GPIO_NUM_22     // SCL
#define I2C_MASTER_SDA_IO   GPIO_NUM_21     // SDA
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

// Cấu hình MQ-9
#define MQ9_ADC_CHANNEL     ADC1_CHANNEL_6  // GPIO34

// Cấu trúc dữ liệu sensor
typedef struct {
    float temperature;      // Nhiệt độ (°C)
    float humidity;         // Độ ẩm (%)
    int co2_level;          // Nồng độ CO2 (ADC raw value)
    float co2_ppm;          // Nồng độ CO2 (ppm - tính toán)
    bool is_valid;          // Dữ liệu có hợp lệ không
} sensor_data_t;

// Khởi tạo sensors
void sensor_init(void);

// Đọc dữ liệu từ SHT31
bool read_sht31(float *temp, float *humi);

// Đọc dữ liệu từ MQ-9
int read_mq9_raw(void);
float read_mq9_ppm(void);

// Đọc tất cả sensors
sensor_data_t read_all_sensors(void);

#endif