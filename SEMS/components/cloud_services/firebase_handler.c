#include "firebase_handler.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_tls.h"
#include "esp_timer.h"

static const char *TAG = "FIREBASE_C";

// Event handler không cần thiết cho client POST đơn giản
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    // Chúng ta không cần xử lý sự kiện trong trường hợp này, chỉ cần kiểm tra mã trả về
    return ESP_OK;
}

void firebase_init(void)
{
    // Không cần khởi tạo gì phức tạp trong C thuần, chỉ cần log
    ESP_LOGI(TAG, "Firebase RTDB initialized for URL: %s", FIREBASE_RTDB_URL);
}

// Hàm này sẽ sử dụng cJSON để tạo payload và esp_http_client để POST
bool firebase_send_sensor_data(sensor_data_t data, const char *room_id)
{
    char post_url[256];
    
    // Xây dựng URL: FIREBASE_RTDB_URL/smarthome/room_id/sensor_logs.json
    // Sử dụng .json để kích hoạt REST API POST
    snprintf(post_url, sizeof(post_url), 
             "%s/smarthome/%s/sensor_logs.json", 
             FIREBASE_RTDB_URL, room_id);
    
    // 1. Tạo JSON Payload
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create cJSON object");
        return false;
    }
    
    long long timestamp = esp_timer_get_time() / 1000000;
    
    cJSON_AddNumberToObject(root, "temp", data.temperature);
    cJSON_AddNumberToObject(root, "humi", data.humidity);
    cJSON_AddNumberToObject(root, "co2_ppm", data.co2_ppm);
    cJSON_AddNumberToObject(root, "timestamp", (double)timestamp);
    
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (post_data == NULL) {
        ESP_LOGE(TAG, "Failed to print cJSON");
        return false;
    }
    
    // 2. Cấu hình HTTP Client
    esp_http_client_config_t config = {
        .url = post_url,
        .event_handler = _http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 3. Gửi POST Request
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        
        // Firebase POST thành công thường trả về mã 200 OK
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "Data sent to Firebase. Status: %d", status_code);
            success = true;
        } else {
            ESP_LOGW(TAG, "Firebase responded with status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    
    // 4. Dọn dẹp
    free(post_data);
    esp_http_client_cleanup(client);
    
    return success;
}
