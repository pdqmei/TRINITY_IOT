// Shared application configuration
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Pin mappings
#define PIN_FAN        16
#define PIN_LED_RED    25
#define PIN_LED_GREEN  26
#define PIN_LED_BLUE   27
#define PIN_BUZZER     19
#define PIN_SHT31_SDA  21
#define PIN_SHT31_SCL  22
#define PIN_MQ135      36

// I2C
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define SHT31_ADDR         0x44

// Timing (milliseconds)
#define SENSOR_READ_INTERVAL_MS 5000
#define MQTT_PUBLISH_INTERVAL_MS 10000

#endif // APP_CONFIG_H
