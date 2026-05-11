#include "wifi_app.h"

#if __has_include("app_config.h")
#include "app_config.h"
#else
#error "Missing app_config.h. Copy include/app_config.example.h to include/app_config.h and fill in your values."
#endif

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"


#define CONNECTED_BIT BIT0
#define FAILED_BIT BIT1
#define MAX_RETRY ((int32_t)5)

static EventGroupHandle_t wifi_event_group;
static int32_t retry_count = 0;

static const char *TAG = "WIFI_APP";

static void handle_wifi_start(void *event_data) {
    (void)event_data;
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "WiFi station started");
}

static void handle_wifi_disconnected(wifi_event_sta_disconnected_t *event_data) {
    ESP_LOGI(TAG, "WiFi disconnected, reason=%" PRIu8, event_data->reason);
    if (retry_count < MAX_RETRY) {
        ESP_LOGI(TAG, "Retrying... (%" PRId32 "/%" PRId32 ")", retry_count + 1, MAX_RETRY);
        ESP_ERROR_CHECK(esp_wifi_connect());
        ++retry_count;
    } else {
        ESP_LOGI(TAG, "WiFi connection failed after retries");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, FAILED_BIT);
    }
}

static void handle_wifi_connected(wifi_event_sta_connected_t *event_data) {
    (void)event_data;
    ESP_LOGI(TAG, "WiFi connected");
}

static void handle_wifi_event(int32_t event_id, void *event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            handle_wifi_start(event_data); 
            break;

        case WIFI_EVENT_STA_DISCONNECTED: 
            handle_wifi_disconnected(event_data); 
            break;

        case WIFI_EVENT_STA_CONNECTED:
            handle_wifi_connected(event_data); 
            break;

        default:
            ESP_LOGI(TAG, "WiFi event received: %" PRId32, event_id);
            break;
    }
}

static void handle_ip_got_ip(ip_event_got_ip_t *event_data) {
    retry_count = 0;
    xEventGroupClearBits(wifi_event_group, FAILED_BIT);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

    ESP_LOGI(TAG, "Got IP address " IPSTR, IP2STR(&event_data->ip_info.ip));
}

static void handle_ip_event(int32_t event_id, void *event_data) {
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            handle_ip_got_ip(event_data);
            break;

        default:
            ESP_LOGI(TAG, "Ip event received: %" PRId32, event_id);
            break;
    }
}

static void event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
    ) {

    (void)arg;

    if (event_base == WIFI_EVENT) {
        handle_wifi_event(event_id, event_data);
    } else if (event_base == IP_EVENT) {
        handle_ip_event(event_id, event_data);
    } else {
        ESP_LOGW(TAG, "Unhandled event base, id=%" PRId32, event_id);
    }
}

esp_err_t wifi_app_connect(void) {

    // Initializes ESP-IDF networking layer.
    ESP_ERROR_CHECK(esp_netif_init());

    // Creates the system event dispatcher (to dispatch wifi events).
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initializes default wifi station network interface.
    esp_netif_t *sta_if = esp_netif_create_default_wifi_sta();
    if (!sta_if) {
        ESP_LOGE(TAG, "Wifi station creation failed!");
        return ESP_FAIL;
    }

    // Initializes wifi driver.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "Wifi driver initialized");

    // Lets app wait for connected/failed result.
    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Wifi event group creation failed!");
        return ESP_FAIL;
    }

    // Registers handler for wifi and ip events.
    esp_event_handler_instance_t wifi_event_instance, ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID, 
            event_handler,
            NULL,
            &wifi_event_instance
        ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            event_handler,
            NULL,
            &ip_event_instance
        ));


    // Puts ESP32 in client (station) mode.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Base configuration (here just set SSID)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Configures WPA2-Enterprise.
    ESP_ERROR_CHECK(esp_eap_client_set_identity(
            (uint8_t*)WIFI_IDENTITY,
            strlen(WIFI_IDENTITY)
        ));
    ESP_ERROR_CHECK(esp_eap_client_set_username(
            (uint8_t*)WIFI_USERNAME,
            strlen(WIFI_USERNAME)
        ));
    ESP_ERROR_CHECK(esp_eap_client_set_password(
            (uint8_t*)WIFI_PASS,
            strlen(WIFI_PASS)
        ));
    ESP_ERROR_CHECK(esp_eap_client_set_eap_methods(ESP_EAP_TYPE_ALL));
    ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());

    // Starts wifi; WIFI_EVENT_STA_START will trigger the first connection attempt.
    ESP_ERROR_CHECK(esp_wifi_start());

    // Waits for event handler to report connected/failed.
    EventBits_t res = xEventGroupWaitBits(
            wifi_event_group,
            CONNECTED_BIT | FAILED_BIT,
            pdTRUE, 
            pdFALSE, 
            portMAX_DELAY
        );

    if (res & FAILED_BIT) return ESP_FAIL;
    return ESP_OK;
}
