idf_component_register(
    SRCS "led.c"
    INCLUDE_DIRS "."
    REQUIRES
    driver
    freertos
)
