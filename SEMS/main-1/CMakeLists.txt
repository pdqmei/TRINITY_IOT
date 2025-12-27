idf_component_register(
    SRCS
        "main.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        sensor_readings
        actuator_control
        wifi_manager
        mqtt_client
        cloud_services
        esp_timer

    PRIV_REQUIRES
        mqtt
)