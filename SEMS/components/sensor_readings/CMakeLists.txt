idf_component_register(
    SRCS "sht31.c" "mq135.c" "sensor_control.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc sensor_types
)