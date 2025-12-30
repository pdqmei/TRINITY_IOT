idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        fan
        led
        whistle
        sht31
        mq135
        wifi_manager
        mqtt_handler
)