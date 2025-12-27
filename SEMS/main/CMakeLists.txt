idf_component_register(
    SRCS
        "main.c"
        "sensor_control.c"
        "device_control.c"
        "wifi_handler.c"
        "mqtt_handler.c"
        "firebase_handler.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_timer
        esp_wifi
        esp_netif
        esp_http_client
        esp-tls
        esp_driver_gpio
        esp_driver_ledc
        nvs_flash
        driver
        esp_adc      
        json

    PRIV_REQUIRES
        mqtt
        esp_driver_i2c
)
