#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

// Cấu hình WiFi
#define WIFI_SSID       "Data"
#define WIFI_PASS       "12345678"

// Khởi tạo WiFi
void wifi_init_sta(void);

// Kiểm tra kết nối
bool wifi_is_connected(void);

#endif
