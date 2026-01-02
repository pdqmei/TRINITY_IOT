/**
 * @file lcd1602.h
 * @brief LCD1602 I2C Driver với PCF8574
 * @note Hỗ trợ hiển thị nhiệt độ, độ ẩm, và chất lượng không khí
 */

#ifndef LCD1602_H
#define LCD1602_H

/**
 * @brief Khởi tạo LCD1602
 * Tự động scan địa chỉ I2C (0x27, 0x3F, 0x20, 0x38)
 */
void lcd_init(void);

/**
 * @brief Xóa màn hình LCD
 */
void lcd_clear(void);

/**
 * @brief Hiển thị nhiệt độ và độ ẩm (legacy function)
 * @param temp Nhiệt độ (°C)
 * @param hum Độ ẩm (%)
 */
void lcd_display(float temp, float hum);

/**
 * @brief Hiển thị đầy đủ 3 thông số: nhiệt độ, độ ẩm, chất lượng không khí
 * @param temp Nhiệt độ (°C)
 * @param hum Độ ẩm (%)
 * @param air_level Mức chất lượng không khí (0=Good, 1=Fair, 2=Moderate, 3=Poor, 4=Very Poor)
 * @param ppm Nồng độ khí PPM từ MQ135 (dùng -1 nếu không có dữ liệu)
 * 
 * Layout LCD 16x2:
 * Row 0: T:23.5C H:65.2%
 * Row 1: AQ:Good   450ppm
 */
void lcd_display_all(float temp, float hum, int air_level, float ppm);

#endif