idf_component_register(
    SRCS "main.c"
         "sensor_control.c"
         "device_control.c"
         "wifi_handler.c"
         "mqtt_handler.c"
         "firebase_handler.c"
    
    INCLUDE_DIRS "."
    
    REQUIRES "esp_http_client" 
             "cJSON" 
             "esp_timer"
)