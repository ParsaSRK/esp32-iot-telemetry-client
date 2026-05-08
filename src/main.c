#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "wifi_app.h"

static const char* TAG = "APP";


void app_main(void) {
    ESP_LOGI(TAG, "Starting wifi setup");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(wifi_app_connect());
}
