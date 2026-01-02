idf_component_register(
    SRCS "lcd1602.c"
    INCLUDE_DIRS "."
    REQUIRES 
        driver       # Cho I2C driver
        esp_timer    # Cho esp_rom_delay_us
    PRIV_REQUIRES
        config       # Cho app_config.h
)
