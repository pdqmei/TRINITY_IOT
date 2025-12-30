idf_component_register(
    SRCS "fan.c" "led.c" "whistle.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_log
)
idf_component_register(
    SRCS "fan.c" "led.c" "whistle.c"
    INCLUDE_DIRS "."
    REQUIRES 
        driver
        freertos
        esp_log
)
