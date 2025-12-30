TRINITY_IOT - Environment Monitor
================================

Tổng quan
---------
Hệ thống giám sát môi trường sử dụng ESP32, đọc SHT31 (nhiệt độ/độ ẩm) và MQ135 (chất lượng không khí), điều khiển quạt, LED RGB và buzzer. Dữ liệu được publish qua MQTT.

Linh kiện (BOM)
- ESP32-DevKitC
- SHT31 (I2C)
- MQ135 (analog)
- MOSFET (IRF540N) + resistor 10k (fan gate)
- Fan DC 5V
- LED RGB 4-pin (common cathode) + 3x 220Ω resistors
- Buzzer active (5V)

Sơ đồ kết nối (ASCII)

    ESP32 GPIO21 ---- SHT31 SDA
    ESP32 GPIO22 ---- SHT31 SCL

    ESP32 GPIO36 (ADC1_CH0) ---- MQ135 analog out

    ESP32 GPIO18 ----| Gate MOSFET (IRF540N) ---- Fan 5V
                     | (10k resistor between GPIO and Gate)

    ESP32 GPIO25 ---- Resistor(220Ω) ---- LED RED
    ESP32 GPIO26 ---- Resistor(220Ω) ---- LED GREEN
    ESP32 GPIO27 ---- Resistor(220Ω) ---- LED BLUE
    LED common cathode ---- GND

    ESP32 GPIO23 ---- Buzzer (active LOW) ---- Buzzer VCC 5V

Build
-----
Configure WiFi credentials in `idf.py menuconfig` or set `CONFIG_ESP_WIFI_SSID`/`CONFIG_ESP_WIFI_PASSWORD` in `sdkconfig.defaults`.

    idf.py menuconfig  # Config WiFi SSID/Pass
    idf.py build
    idf.py -p /dev/ttyUSB0 flash
    idf.py -p /dev/ttyUSB0 monitor

MQTT Topics & Payload
- Publish every 10s:
  - `sensor/temperature` -> {"value": 25.5, "unit": "C"}
  - `sensor/humidity` -> {"value": 60, "unit": "%"}
  - `sensor/air_quality` -> {"level": 2}
  - `status/fan` -> {"state": "on"/"off"}
  - `status/led` -> {"color": {"r":255,"g":165,"b":0}}

- Subscribe:
  - `control/fan` -> {"command":"on"/"off"/"auto"}
  - `control/led` -> {"r":255,"g":0,"b":0}
  - `system/reset` -> {"action":"reboot"}

LED color meanings
- Level 0: Green
- Level 1: Yellow
- Level 2: Orange
- Level 3: Red (blink)
- Level 4: Purple (fast blink)
Overlay: Temp >30°C breathing red; >35°C fast blink

Buzzer patterns
- Startup: 1 long beep 500ms
- WiFi connected: 2 short beeps
- WiFi failed: 3 quick beeps
- Air quality >=3: beep every 5s (100ms)
- Temp >35°C: rapid beep (50ms on/off)

Troubleshooting
- Nếu WiFi không connect: kiểm tra SSID/Password, thử `idf.py monitor` để xem logs.
- Nếu MQTT không publish: kiểm tra broker URI, firewall.

License: MIT
# TRINITY_IOT
SMART ENVIRONMENT WITH ESP32
