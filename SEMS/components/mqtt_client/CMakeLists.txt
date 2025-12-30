idf_component_register(
    SRCS "mqtt_handler.c"
    INCLUDE_DIRS "."
    REQUIRES mqtt esp_event json sensor_types
)