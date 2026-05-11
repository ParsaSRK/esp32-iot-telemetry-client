#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "wifi_app.h"
#include "telemetry_client.h"
#include "dht11_sensor.h"


static const char *TAG = "APP";

void app_main(void) {

    ESP_LOGI(TAG, "Starting...");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(wifi_app_connect());

    ESP_ERROR_CHECK(telemetry_client_check_server());

    while (true) {
        dht11_reading_t reading;

        esp_err_t err = dht11_sensor_read(&reading);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature=%.1fC Humidity=%.1f%%", reading.temperature_c, reading.humidity_percent);
            ESP_ERROR_CHECK(telemetry_client_send_report(&reading));
        } else {
            ESP_LOGE(TAG, "DHT11 read failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
