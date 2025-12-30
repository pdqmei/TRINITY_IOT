idf_component_register(
    SRCS "sht31.c" "mq135.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_log esp_adc_cal
)
idf_component_register(
    SRCS "sht31.c" "mq135.c"
    INCLUDE_DIRS "."
    REQUIRES 
        driver
        esp_log
        esp_adc_cal
)
