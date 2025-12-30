#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#include "sensor_types.h"

// Cấu hình Firebase Realtime Database
#define FIREBASE_RTDB_URL "https://sems-66e00-default-rtdb.firebaseio.com"

// Khởi tạo Firebase (chủ yếu là để khởi tạo các thông số)
void firebase_init(void);

// Gửi dữ liệu cảm biến lên Firebase
bool firebase_send_sensor_data(sensor_data_t data, const char *room_id);

#endif
