// Application configuration and pin definitions
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Pins (as requested)
#define PIN_SHT31_SDA 21
#define PIN_SHT31_SCL 22
#define PIN_MQ135_ADC_CHANNEL ADC1_CHANNEL_0 // GPIO36

#define PIN_FAN GPIO_NUM_16
#define PIN_LED_R GPIO_NUM_25
#define PIN_LED_G GPIO_NUM_26
#define PIN_LED_B GPIO_NUM_27
#define PIN_BUZZER GPIO_NUM_17 // active LOW

// MQTT Broker (public test broker)
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#define MQTT_BROKER_PORT 1883

// Other defaults
#define SENSOR_READ_INTERVAL_MS 5000
#define MQTT_PUBLISH_INTERVAL_MS 10000

#endif
