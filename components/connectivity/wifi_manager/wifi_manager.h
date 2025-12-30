#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

void wifi_init(void); // Hàm để main gọi khởi tạo WiFi
bool wifi_is_connected(void); 

#endif
