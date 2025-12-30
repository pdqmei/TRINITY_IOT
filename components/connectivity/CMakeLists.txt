idf_component_register(
    SRCS "mqtt_handler.c" "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_mqtt
        esp_event
        esp_log
        driver
        freertos
)
