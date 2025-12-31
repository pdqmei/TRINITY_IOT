#ifndef SHT31_H
#define SHT31_H

#include <stdint.h>

typedef struct {
    float temperature;
    float humidity;
} sht31_data_t;

void sht31_init(void);
sht31_data_t sht31_read(void);
float sht31_get_temperature(void);
float sht31_get_humidity(void);

#endif
