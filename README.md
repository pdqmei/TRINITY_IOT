# TRINITY IOT - Smart Environment Monitor
**Developer:** pdqmei  
**Repository:** [github.com/pdqmei/TRINITY_IOT](https://github.com/pdqmei/TRINITY_IOT)

## 📋 Tổng Quan
Hệ thống giám sát môi trường thông minh sử dụng ESP32 với khả năng:
- Đo nhiệt độ/độ ẩm (SHT31) và chất lượng không khí (MQ135)
- Điều khiển quạt tự động 3 cấp tốc độ (PWM)
- Hiển thị trạng thái qua LED RGB 5 cấp độ
- Cảnh báo qua buzzer 4 mức độ
- Hiển thị thông tin trên LCD1602 I2C
- Kết nối MQTT với LED báo trạng thái
- Kiến trúc FreeRTOS với 4 task độc lập

## 🛠️ Danh Sách Linh Kiện

### Vi điều khiển & Cảm biến
- **ESP32 DevKit V1** - Board chính
- **SHT31** - Cảm biến nhiệt độ/độ ẩm (I2C)
- **MQ135** - Cảm biến chất lượng không khí (ADC)
- **LCD1602 I2C** - Màn hình hiển thị

### Actuators
- **IRF520 MOSFET Module** hoặc **L298N** - Điều khiển quạt DC
- **Fan DC 12V** - Quạt làm mát (qua boost converter MT3608)
- **LED RGB Common Cathode** - 4 chân (R/G/B/GND)
- **Buzzer 5V Active** - Cảnh báo âm thanh
- **LED đỏ 5mm** - Báo trạng thái MQTT

### Điện trở & Nguồn
- **3x Điện trở 220Ω** - Cho LED RGB
- **1x Điện trở 220Ω** - Cho LED MQTT
- **MT3608 Boost Converter** - Tăng áp 5V→12V cho quạt
- **Nguồn 5V/2A** - Cấp nguồn cho ESP32

## 🔌 Sơ Đồ Kết Nối

### I2C Bus (SHT31 & LCD)
```
ESP32 GPIO21 (SDA) ──→ SHT31 SDA ──→ LCD SDA
ESP32 GPIO22 (SCL) ──→ SHT31 SCL ──→ LCD SCL
```

### MQ135 Air Quality Sensor
```
ESP32 GPIO36 (ADC1_CH0) ──→ MQ135 A0
MQ135 VCC ──→ 5V
MQ135 GND ──→ GND
```

### Fan Control (IRF520 Module)
```
ESP32 GPIO23 (PWM) ──→ IRF520 SIG
IRF520 VIN ──→ MT3608 OUT+ (12V)
IRF520 V+ ──→ Fan dây đỏ
IRF520 V- ──→ Fan dây đen
IRF520 GND ──→ ESP32 GND (COMMON GND - QUAN TRỌNG!)
MT3608 IN+ ──→ 5V
MT3608 IN- ──→ GND
```

### LED RGB
```
ESP32 GPIO25 ──→ 220Ω ──→ LED Red
ESP32 GPIO26 ──→ 220Ω ──→ LED Green
ESP32 GPIO27 ──→ 220Ω ──→ LED Blue
LED Common Cathode ──→ GND
```

### Buzzer & MQTT LED
```
ESP32 GPIO19 ──→ Buzzer (+)
Buzzer (-) ──→ GND

ESP32 GPIO18 ──→ 220Ω ──→ LED MQTT Anode
LED MQTT Cathode ──→ GND
```

## ⚙️ Cấu Hình ESP-IDF

### Cài Đặt ESP-IDF
```bash
# Windows PowerShell
idf.py menuconfig
```

### WiFi Configuration
Trong `menuconfig`:
- Component config → Example Connection Configuration
  - WiFi SSID: Tên WiFi của bạn
  - WiFi Password: Mật khẩu WiFi

Hoặc chỉnh sửa `sdkconfig.defaults`:
```
CONFIG_ESP_WIFI_SSID="YourWiFiName"
CONFIG_ESP_WIFI_PASSWORD="YourPassword"
```

### MQTT Configuration
Chỉnh sửa trong `components/connectivity/mqtt_handler.c`:
```c
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
```

## 🔨 Build & Flash

### Build Project
```bash
idf.py build
```

### Flash to ESP32
```bash
idf.py -p COM3 flash
```

### Monitor Serial Output
```bash
idf.py -p COM3 monitor
```

### Build + Flash + Monitor (All-in-one)
```bash
idf.py -p COM3 build flash monitor
```

## 📊 Hoạt Động Hệ Thống

### Fan Control (3 Levels)
| Nhiệt độ | Tốc độ | Duty PWM |
|----------|--------|----------|
| < 25°C   | OFF    | 0%       |
| 25-30°C  | MEDIUM | 50%      |
| ≥ 30°C   | HIGH   | 100%     |

### LED RGB Indicators (5 Levels)
| Air Quality Level | Màu sắc | Mô tả | Raw ADC Range |
|-------------------|---------|-------|---------------|
| 0 - Good | 🟢 Green | Không khí tốt | < 600 |
| 1 - Fair | 🔵 Cyan | Khá tốt | 600-899 |
| 2 - Moderate | 🟡 Yellow | Trung bình | 900-1299 |
| 3 - Poor | 🔴 Red (blink) | Kém | 1300-1799 |
| 4 - Very Poor | 🟣 Purple (fast blink) | Rất kém | ≥ 1800 |

### Buzzer Alerts (4 Levels)
| Level | Điều kiện | Pattern | Mô tả |
|-------|-----------|---------|-------|
| 0 | Normal | Silent | Không cảnh báo |
| 1 | Temp > 28°C hoặc AQ = 2 | Beep 5s | 100ms ON, 5s interval |
| 2 | AQ ≥ 3 | Beep 2s (5 lần) | 100ms ON, 2s interval, 5 times |
| 3 | Temp > 33°C | Beep 1s (10 lần) | 100ms ON, 1s interval, 10 times |

**Priority:** Level 3 > Level 2 > Level 1

### LCD Display Format
```
Line 1: T:25.5C H:60%
Line 2: AQ:2 PPM:450
```
- AQ: Air Quality Level (0-4)
- PPM: CO₂ equivalent (nếu có)

### MQTT Topics

#### Published (every 5 seconds)
```json
sensor/temperature    → {"value": 25.5, "unit": "C"}
sensor/humidity       → {"value": 60.0, "unit": "%"}
sensor/air_quality    → {"level": 2, "raw": 1024, "ppm": 450.5}
status/fan           → {"state": "on", "speed": 50}
status/led           → {"color": {"r": 255, "g": 255, "b": 0}}
status/buzzer        → {"level": 1, "active": true}
```

## 🐛 Troubleshooting

### Fan không hoạt động hoặc chạy liên tục
1. **Kiểm tra GND chung:** ESP32 GND phải nối với IRF520 GND và MT3608 GND
2. **Kiểm tra dây quạt:** Dây đỏ → V+ (KHÔNG phải VIN), dây đen → V-
3. **Kiểm tra bypass:** Đảm bảo quạt KHÔNG nối trực tiếp VIN
4. **Pull-down resistor:** Thêm 10kΩ từ GPIO23 xuống GND nếu cần

### MQ135 hiển thị "N/A"
1. **Warmup time:** Chờ 30-60s sau khi bật nguồn
2. **Kiểm tra kết nối:** GPIO36 → MQ135 A0, VCC → 5V
3. **Sensor validation:** Raw ADC phải trong khoảng 50-4000
4. **Calibration:** Chạy `mq135_calibrate()` trong môi trường sạch

### WiFi không kết nối
1. Kiểm tra SSID/Password trong `sdkconfig`
2. Đảm bảo WiFi 2.4GHz (ESP32 không hỗ trợ 5GHz)
3. Xem logs: `idf.py monitor`

### MQTT không publish
1. Kiểm tra broker URI trong `mqtt_handler.c`
2. Test broker: `mqtt://broker.hivemq.com:1883`
3. Kiểm tra firewall/network

### Build errors
```bash
# Clean build
idf.py fullclean
idf.py build

# Nếu thiếu dependencies
git submodule update --init --recursive
```

## 📁 Cấu Trúc Project

```
TRINITY_IOT/
├── main/
│   ├── main.c              # Core application (4 FreeRTOS tasks)
│   └── CMakeLists.txt
├── components/
│   ├── actuators/
│   │   ├── buzzer/         # Buzzer control
│   │   ├── fan/            # PWM fan control
│   │   └── led/            # RGB LED control
│   ├── sensors/
│   │   ├── sht31/          # Temperature/Humidity
│   │   └── mq135/          # Air quality + calibration
│   ├── connectivity/
│   │   └── mqtt_handler/   # MQTT client
│   ├── utils/
│   │   ├── moving_average/ # Signal filtering
│   │   └── lcd_handler/    # LCD1602 I2C display
│   └── config/
│       └── app_config.h    # GPIO pin definitions
└── CMakeLists.txt
```

## 🚀 Tính Năng Nâng Cao

### Moving Average Filter
- **10-sample buffer** cho SHT31 (nhiệt độ/độ ẩm)
- **10-sample buffer** cho MQ135 (air quality level)
- Giảm nhiễu, ổn định đọc giá trị

### MQ135 Calibration
- Tự động calibrate trong môi trường sạch
- Lưu R0 vào NVS (non-volatile storage)
- Fallback: Ước tính PPM từ raw ADC nếu chưa calibrate

### Component-Based Architecture
- Tách biệt logic sensor/actuator
- Dễ dàng thêm/sửa components
- CMakeLists.txt dependencies rõ ràng

## 📝 License & Credits

**License:** MIT  
**Developer:** pdqmei  
**ESP-IDF Version:** v5.4.2  
**GitHub:** [github.com/pdqmei/TRINITY_IOT](https://github.com/pdqmei/TRINITY_IOT)

---

### 🔧 Known Issues
- IRF520 module có pull-up internal → cần pull-down 10kΩ external
- MQ135 cần warmup 30-60s để đọc chính xác
- LCD I2C address mặc định 0x27 (có thể cần scan I2C)

### 📞 Support
Nếu gặp vấn đề, vui lòng mở issue trên GitHub hoặc kiểm tra logs qua serial monitor.

