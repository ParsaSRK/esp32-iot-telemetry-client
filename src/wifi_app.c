#include "wifi_app.h"

#if __has_include("wifi_secret.h")
#include "wifi_secret.h"
#else
#error "Missing include/wifi_secret.h. Copy include/wifi_secret.example.h "
       "to include/wifi_secret.h and fill in your WiFi credentials."
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

static const char *TAG = "WIFI_APP";
static EventGroupHandle_t wifi_event_group;
static int32_t retry_count = 0;


static void event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
    ) {

    (void)arg;

    if (event_base == WIFI_EVENT) {

        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_ERROR_CHECK(esp_wifi_connect());
                ESP_LOGI(TAG, "WiFi station started");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected, reason=%" PRIu8, ((wifi_event_sta_disconnected_t *)event_data)->reason);
                if (retry_count < MAX_RETRY) {
                    ESP_LOGI(TAG, "Retrying... (%" PRId32 "/%" PRId32 ")", retry_count + 1, MAX_RETRY);
                    ESP_ERROR_CHECK(esp_wifi_connect());
                    ++retry_count;
                } else {
                    ESP_LOGI(TAG, "WiFi connection failed after retries");
                    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
                    xEventGroupSetBits(wifi_event_group, FAILED_BIT);
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            default:
                ESP_LOGI(TAG, "WiFi event received: %" PRId32, event_id);
                break;
        }

    } else if (event_base == IP_EVENT) {

        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                retry_count = 0;
                xEventGroupClearBits(wifi_event_group, FAILED_BIT);
                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

                ESP_LOGI(TAG, "Got IP address " IPSTR, IP2STR( &((ip_event_got_ip_t *)event_data)->ip_info.ip ));
                break;
            default:
                ESP_LOGI(TAG, "Ip event received: %" PRId32, event_id);
                break;
        }

    } else {
        ESP_LOGE(TAG, "Unknown event received: %" PRId32, event_id);
    }
}

esp_err_t wifi_app_connect(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_if = esp_netif_create_default_wifi_sta();
    if (!sta_if) {
        ESP_LOGE(TAG, "Wifi station creation failed!");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "Wifi driver initialized");

    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Wifi event group creation failed!");
        return ESP_FAIL;
    }

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


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

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

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t res = xEventGroupWaitBits(
            wifi_event_group,
            CONNECTED_BIT | FAILED_BIT,
            pdTRUE, 
            pdFALSE, 
            portMAX_DELAY
        );

    if (res & FAILED_BIT)
        return ESP_FAIL;

    return ESP_OK;
}
