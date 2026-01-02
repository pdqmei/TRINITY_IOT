idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        fan
        led
        buzzer
        sht31
        mq135
        lcd1602
        moving_average
        wifi_manager
        mqtt_handler
)