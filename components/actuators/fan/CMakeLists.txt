idf_component_register(
    SRCS "fan.c"
    INCLUDE_DIRS "."
    REQUIRES
    config
    driver
    freertos
)
