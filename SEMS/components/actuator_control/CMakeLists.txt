idf_component_register(
    SRCS "led.c" "fan.c" "device_control.c" "whistle.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_ledc
)