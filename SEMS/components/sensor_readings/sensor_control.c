#include "sensor_control.h"
#include "mq135.h"
#include "sht31.h"

void sensor_control_init(void)
{
    sht31_init();
    mq135_init();
}

sensor_data_t sensor_read_all(void)
{
    sensor_data_t data = {0};

    data.is_valid = sht31_read(&data.temperature, &data.humidity);
    data.co2_raw  = mq135_read_raw();
    data.co2_ppm  = mq135_read_voltage();

    return data;
}
