/* Active buzzer controlled by GPIO (active LOW): LOW = ON, HIGH = OFF
 * Pin: PIN_BUZZER (see app_config.h)
 * 
 * Pattern-based control using FreeRTOS Task Notification for instant response
 */

#include "buzzer.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BUZZER";

#define BUZZER_PIN PIN_BUZZER
#define BEEP_DURATION_MS 100  // Thời gian tít (ms)

// Task handle và level
static TaskHandle_t buzzer_task_handle = NULL;
static volatile int current_level = 0;

// Forward declaration
static void buzzer_pattern_task(void *pvParameters);

void buzzer_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    // Active LOW -> set HIGH to keep buzzer off
    gpio_set_level(BUZZER_PIN, 1);
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO%d (active LOW: 0=ON, 1=OFF)", BUZZER_PIN);
}

void buzzer_on(void) {
    gpio_set_level(BUZZER_PIN, 0); // Active LOW: 0 = ON (kêu)
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_PIN, 1); // Active LOW: 1 = OFF (không kêu)
}

void buzzer_beep(int count) {
    for (int i = 0; i < count; i++) {
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(150));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ========== PATTERN TASK với Task Notification ==========
static void buzzer_pattern_task(void *pvParameters)
{
    int level = current_level;
    uint32_t wait_ms = 0;
    uint32_t notify_value = 0;
    
    ESP_LOGI(TAG, "Pattern task started, level=%d", level);
    
    while (1) {
        // Kiểm tra level, nếu 0 thì thoát
        level = current_level;
        if (level == 0) {
            break;
        }
        
        // Xác định thời gian nghỉ theo level
        switch (level) {
            case 1: wait_ms = 5000; break;  // 5 giây
            case 2: wait_ms = 3000; break;  // 3 giây
            case 3: wait_ms = 1000; break;  // 1 giây
            default: wait_ms = 1000; break;
        }
        
        // === BEEP ===
        buzzer_on();
        
        // Chờ với notification - có thể bị interrupt ngay lập tức
        if (xTaskNotifyWait(0, ULONG_MAX, &notify_value, pdMS_TO_TICKS(BEEP_DURATION_MS)) == pdTRUE) {
            // Nhận được notification -> level thay đổi, xử lý ngay
            buzzer_off();
            ESP_LOGI(TAG, "Level changed during beep -> %d", current_level);
            continue;  // Quay lại đầu loop để check level mới
        }
        
        buzzer_off();
        
        // === WAIT (nghỉ) ===
        // Chờ với notification - có thể bị interrupt ngay lập tức  
        if (xTaskNotifyWait(0, ULONG_MAX, &notify_value, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
            // Nhận được notification -> level thay đổi
            ESP_LOGI(TAG, "Level changed during wait -> %d", current_level);
            continue;  // Quay lại đầu loop để check level mới
        }
    }
    
    buzzer_off();
    ESP_LOGI(TAG, "Pattern task stopped");
    buzzer_task_handle = NULL;
    vTaskDelete(NULL);
}

void buzzer_set_level(int level) {
    ESP_LOGI(TAG, "Set level: %d -> %d", current_level, level);
    
    // Cập nhật level
    int old_level = current_level;
    current_level = level;
    
    if (level <= 0) {
        // Tắt buzzer
        current_level = 0;
        buzzer_off();
        
        // Notify task để nó thoát ngay
        if (buzzer_task_handle != NULL) {
            xTaskNotifyGive(buzzer_task_handle);
        }
        return;
    }
    
    // Level > 0: Cần chạy pattern
    if (buzzer_task_handle == NULL) {
        // Tạo task mới
        xTaskCreate(buzzer_pattern_task, "buzzer_pattern", 2048, NULL, 6, &buzzer_task_handle);
        ESP_LOGI(TAG, "Created pattern task for level %d", level);
    } else if (old_level != level) {
        // Task đang chạy, notify để cập nhật level ngay
        xTaskNotifyGive(buzzer_task_handle);
        ESP_LOGI(TAG, "Notified task: level changed to %d", level);
    }
}

int buzzer_get_level(void) {
    return current_level;
}

bool buzzer_is_running(void) {
    return (buzzer_task_handle != NULL && current_level > 0);
}