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
    char url[256];

    // URL động theo phòng
    snprintf(url, sizeof(url),
             "%s/smarthome/%s/sensors.json",
             FIREBASE_RTDB_URL, room_id);

    // Tạo JSON
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddNumberToObject(root, "temp", data.temperature);
    cJSON_AddNumberToObject(root, "humi", data.humidity);
    cJSON_AddNumberToObject(root, "co2", data.co2_ppm);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) return false;

    esp_http_client_config_t config = {
        .url = url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);

    free(json);
    esp_http_client_cleanup(client);

    return (err == ESP_OK);
}

