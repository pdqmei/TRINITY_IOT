/**
 * @file lcd1602.c
 * @brief LCD1602 with PCF8574 - Display Temperature, Humidity, Air Quality
 * Layout:
 * Row 0: T:23.5C H:65.2%
 * Row 1: AQ:Good 450ppm
 */

#include "lcd1602.h"
#include "driver/i2c.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LCD";

#define LCD_BACKLIGHT 0x08
#define En 0x04
#define Rs 0x01

// I2C configuration for LCD (shares I2C bus with SHT31)
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_SDA_IO  PIN_SHT31_SDA
#define I2C_MASTER_SCL_IO  PIN_SHT31_SCL

// Note: I2C bus initialized by sht31_init() at 100kHz
// LCD shares the same I2C bus with SHT31 (0x44) and uses PCF8574 (0x27)

static uint8_t lcd_address = 0x27;
static bool lcd_initialized = false;

// Forward declarations
static void lcd_set_cursor(uint8_t row, uint8_t col);
static void lcd_print(const char *str);

// I2C Scanner
static bool i2c_scan_address(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static void lcd_write_nibble(uint8_t data)
{
    uint8_t buf = data | LCD_BACKLIGHT;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd_address << 1) | I2C_MASTER_WRITE, true);
    
    // Pulse EN: Low -> High -> Low
    i2c_master_write_byte(cmd, buf, true);           // EN low
    i2c_master_write_byte(cmd, buf | En, true);      // EN high
    i2c_master_write_byte(cmd, buf, true);           // EN low
    
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    esp_rom_delay_us(50);
}

static void lcd_send_byte(uint8_t data, uint8_t mode)
{
    uint8_t high = (data & 0xF0) | mode;
    uint8_t low = ((data << 4) & 0xF0) | mode;
    lcd_write_nibble(high);
    lcd_write_nibble(low);
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_send_byte(cmd, 0);
    if (cmd <= 0x03) {
        vTaskDelay(pdMS_TO_TICKS(5));
    } else {
        esp_rom_delay_us(100);
    }
}

static void lcd_data(uint8_t data)
{
    lcd_send_byte(data, Rs);
    esp_rom_delay_us(50);
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t pos = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_cmd(pos);
}

static void lcd_print(const char *str)
{
    while (*str) {
        lcd_data(*str++);
    }
}

void lcd_init(void)
{
    // Avoid re-initializing if already done
    if (lcd_initialized) {
        ESP_LOGI(TAG, "LCD already initialized");
        return;
    }

    ESP_LOGI(TAG, "Scanning I2C bus for LCD...");
    
    // Scan common addresses for LCD with PCF8574
    uint8_t addresses[] = {0x27, 0x3F, 0x20, 0x38};
    bool found = false;
    
    for (int i = 0; i < 4; i++) {
        if (i2c_scan_address(addresses[i])) {
            lcd_address = addresses[i];
            found = true;
            ESP_LOGI(TAG, "LCD found at 0x%02X", lcd_address);
            break;
        }
    }
    
    if (!found) {
        ESP_LOGE(TAG, "LCD not found! Check wiring and I2C address!");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing LCD at 0x%02X...", lcd_address);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Initialization sequence for 4-bit mode
    lcd_write_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    lcd_write_nibble(0x30);
    esp_rom_delay_us(150);
    
    lcd_write_nibble(0x30);
    esp_rom_delay_us(150);
    
    lcd_write_nibble(0x20);
    esp_rom_delay_us(150);
    
    // Configure LCD settings
    lcd_cmd(0x28);  // 4-bit, 2 lines, 5x8
    lcd_cmd(0x08);  // Display OFF
    lcd_cmd(0x01);  // Clear display
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_cmd(0x06);  // Entry mode: increment address, no shift
    lcd_cmd(0x0C);  // Display ON, cursor OFF
    
    lcd_initialized = true;
    ESP_LOGI(TAG, "LCD initialized successfully!");
    
    // Test message
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("TRINITY IoT");
    lcd_set_cursor(1, 0);
    lcd_print("Env Monitor V1");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void lcd_clear(void)
{
    if (!lcd_initialized) {
        ESP_LOGW(TAG, "LCD not initialized");
        return;
    }
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

/**
 * @brief Display temperature, humidity and air quality
 * @param temp Temperature in Celsius
 * @param hum Humidity in percentage
 * @param air_level Air quality level (0-4)
 * @param ppm PPM value from MQ135 (use -1 if not available)
 * 
 * Layout (16 chars per row):
 * Row 0: T:23.5C H:65.2%
 * Row 1: AQ:Good   450ppm
 *        AQ:Fair   580ppm
 *        AQ:Mod    750ppm
 *        AQ:Poor   950ppm
 *        AQ:V.Poor 1200p
 */
void lcd_display_all(float temp, float hum, int air_level, float ppm)
{
    if (!lcd_initialized) {
        ESP_LOGW(TAG, "LCD not initialized");
        return;
    }

    char line1[17], line2[17];
    
    // Row 0: Temperature and Humidity
    // Format: "T:23.5C H:65.2%"
    snprintf(line1, sizeof(line1), "T:%.1fC H:%.1f%%", temp, hum);
    
    // Row 1: Air Quality with PPM
    const char* aq_labels[] = {"Good", "Fair", "Mod", "Poor", "V.Poor"};
    
    if (air_level < 0 || air_level > 4) {
        air_level = 0;
    }
    
    if (ppm >= 0.0f && ppm < 10000.0f) {
        // Show PPM if available and valid
        // Format: "AQ:Good   450ppm" (16 chars total)
        snprintf(line2, sizeof(line2), "AQ:%-6s%4.0fp", aq_labels[air_level], ppm);
    } else {
        // PPM not available: "AQ:Good        "
        snprintf(line2, sizeof(line2), "AQ:%-12s", aq_labels[air_level]);
    }
    
    // Update display
    lcd_set_cursor(0, 0);
    lcd_print(line1);
    
    lcd_set_cursor(1, 0);
    lcd_print(line2);
    
    ESP_LOGD(TAG, "Display: T=%.1f°C H=%.1f%% AQ=%s PPM=%.0f", 
             temp, hum, aq_labels[air_level], ppm);
}

/**
 * @brief Legacy function - now uses lcd_display_all
 */
void lcd_display(float temp, float hum)
{
    lcd_display_all(temp, hum, 0, -1.0f);
}