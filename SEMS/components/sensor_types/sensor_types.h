#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdbool.h>

typedef struct {
    float temperature;      // °C
    float humidity;         // %
    int   co2_raw;          // raw ADC / sensor value (optional)
    float co2_ppm;          // ppm
    bool  is_valid;         // dữ liệu hợp lệ
} sensor_data_t;

#endif // SENSOR_TYPES_H
