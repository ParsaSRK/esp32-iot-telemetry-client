#include "telemetry_client.h"

#if __has_include("app_config.h")
#include "app_config.h"
#else
#error "Missing app_config.h. Copy include/app_config.example.h to include/app_config.h and fill in your values."
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "cJSON.h"

static const char *TAG = "TELEMETRY_CLIENT";

static int sequence = 1;

esp_err_t telemetry_client_check_server(void) {
    ESP_LOGI(TAG, "Checking telemetry server: %s", HEALTH_URL);
    int status_code = 0;

    esp_err_t result = ESP_OK;

    esp_http_client_config_t config = {
        .url = HEALTH_URL,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client_handle = esp_http_client_init(&config);
    if (client_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }

    status_code = esp_http_client_get_status_code(client_handle);

    /* The health endpoint intentionally returns no body on success. */
    if (status_code != 204) {
        ESP_LOGE(TAG, "Telemetry health check failed: status: %d", status_code);
        result = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    esp_http_client_cleanup(client_handle);

    if (result == ESP_OK)
        ESP_LOGI(TAG, "Telemetry server healthy: status=%d", status_code);
    return result;
}

esp_err_t telemetry_client_send_report(const dht11_reading_t *reading) {
    if (reading == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t result = ESP_OK;
    cJSON *root = NULL;
    char *json = NULL;
    esp_http_client_handle_t client_handle = NULL;

    root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Couldn't create JSON object!");
        result = ESP_FAIL;
        goto cleanup;
    }

    int64_t uptime = esp_timer_get_time() / 1000;
    wifi_ap_record_t sta_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&sta_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi AP info: %s", esp_err_to_name(err));
        result = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGI(
            TAG,
            "Sending telemetry report: sequence=%d uptime_ms=%" PRId64
            " wifi_rssi=%d temperature_c=%.1f humidity_percent=%.1f",
            sequence,
            uptime,
            sta_info.rssi,
            reading->temperature_c,
            reading->humidity_percent
        );

    /* Report schema expected by POST /telemetry. */
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
    cJSON_AddNumberToObject(root, "sequence", sequence);
    cJSON_AddNumberToObject(root, "uptime_ms", uptime);
    cJSON_AddNumberToObject(root, "wifi_rssi", sta_info.rssi);
    cJSON_AddNumberToObject(root, "temperature_c", reading->temperature_c);
    cJSON_AddNumberToObject(root, "humidity_percent", reading->humidity_percent);

    /* esp_http_client keeps a pointer to this body until perform() finishes. */
    json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to generate JSON string!");
        result = ESP_FAIL;
        goto cleanup;
    }

    esp_http_client_config_t config = {
        .url = TELEMETRY_URL,
        .method = HTTP_METHOD_POST,
    };
    client_handle = esp_http_client_init(&config);
    if (client_handle == NULL) {
        ESP_LOGE(TAG, "HTTP client initialization failed!");
        result = ESP_FAIL;
        goto cleanup;
    }

    err = esp_http_client_set_header(client_handle, "Content-Type", "application/json");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }

    err = esp_http_client_set_post_field(client_handle, json, strlen(json));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST body: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }

    err = esp_http_client_perform(client_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client_handle);

    /* The backend acknowledges accepted telemetry with HTTP 200. */
    if (status_code != 200) {
        ESP_LOGE(TAG, "Telemetry report rejected: status=%d", status_code);
        result = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Telemetry report accepted: sequence=%d status=%d", sequence, status_code);
    ++sequence;

cleanup:
    if (json) cJSON_free(json);
    if (root) cJSON_Delete(root);
    if (client_handle) esp_http_client_cleanup(client_handle);

    return result;
}
