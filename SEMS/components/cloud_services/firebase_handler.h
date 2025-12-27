#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#include <stdbool.h>

// Định nghĩa cấu trúc sensor_data_t (từ sensor_control)
typedef struct {
    float temperature;      // Nhiệt độ (°C)
    float humidity;         // Độ ẩm (%)
    int co2_level;          // Nồng độ CO2 (ADC raw value)
    float co2_ppm;          // Nồng độ CO2 (ppm)
    bool is_valid;          // Dữ liệu có hợp lệ không
} sensor_data_t;

// Cấu hình Firebase Realtime Database
#define FIREBASE_RTDB_URL "https://sems-66e00-default-rtdb.firebaseio.com"

// Khởi tạo Firebase (chủ yếu là để khởi tạo các thông số)
void firebase_init(void);

// Gửi dữ liệu cảm biến lên Firebase
bool firebase_send_sensor_data(sensor_data_t data, const char *room_id);

#endif
