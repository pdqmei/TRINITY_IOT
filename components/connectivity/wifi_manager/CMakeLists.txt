idf_component_register(
    SRCS "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
    mqtt
    esp_event
    driver
    freertos
)
