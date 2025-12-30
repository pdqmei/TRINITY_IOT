# TRINITY_IOT Project Configuration

## Project Info
This is a Smart Environment Control System using ESP32 with the following features:
- Temperature & Humidity Monitoring (SHT31)
- Air Quality Monitoring (MQ135)
- Automated Fan Control
- Status LED Indicator
- WiFi Connectivity
- MQTT Data Publishing

## Directory Structure
```
TRINITY_IOT/
├── main/
│   ├── main.c                 # Main application
│   ├── CMakeLists.txt         # Main component CMake
│   └── idf_component.yml      # Component definition
├── components/
│   ├── actuator_control/      # Actuators (Fan, LED, Whistle)
│   ├── sensor_readings/       # Sensors (SHT31, MQ135)
│   ├── wifi_manager/          # WiFi handler
│   └── mqtt_client/           # MQTT handler
├── CMakeLists.txt             # Root CMake
└── sdkconfig.defaults         # Default SDK configuration
```

## Hardware Connections
- Fan PWM: GPIO 18
- LED: GPIO 19
- Whistle/Buzzer: GPIO 21
- SHT31 I2C: SDA=GPIO 21, SCL=GPIO 22
- MQ135 ADC: GPIO 36 (ADC1_0)

## WiFi & MQTT
- Update YOUR_SSID and YOUR_PASSWORD in wifi_manager.c
- Default MQTT Broker: mqtt://broker.hivemq.com:1883

## Building
```bash
idf.py build
idf.py flash
idf.py monitor
```

## Features
- Auto fan speed based on temperature (30°C=100%, 25°C=50%)
- LED indicator based on air quality level
- Startup beep sequence
- WiFi auto-reconnect
- MQTT data publishing every 5 seconds
