idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES actuator_control sensor_readings wifi_manager 
             cloud_services mqtt_client nvs_flash sensor_types
)