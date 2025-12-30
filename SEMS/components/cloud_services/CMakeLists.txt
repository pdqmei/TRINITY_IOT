idf_component_register(
    SRCS "firebase_handler.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client json esp_event mbedtls esp-tls sensor_types
    PRIV_REQUIRES esp_timer
)