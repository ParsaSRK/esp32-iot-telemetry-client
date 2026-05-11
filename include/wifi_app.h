#pragma once

#include "esp_err.h"

/**
 * @brief Connects the ESP32 to the configured Wi-Fi network.
 *
 * Initializes the ESP-IDF network stack, starts Wi-Fi station mode, configures
 * WPA2-Enterprise credentials from app_config.h, and blocks until the device
 * either receives an IP address or exhausts its retry attempts.
 *
 * @return ESP_OK If connected and assigned an IP address, ESP_FAIL otherwise.
 */
esp_err_t wifi_app_connect(void);
