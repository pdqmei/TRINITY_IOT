#ifndef SENSOR_CONTROL_H
#define SENSOR_CONTROL_H

#include "sensor_types.h"

void sensor_control_init(void);
sensor_data_t sensor_read_all(void);

#endif
