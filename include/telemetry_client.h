#pragma once

#include "dht11_sensor.h"
#include "esp_err.h"

/**
 * @brief Check whether the configured telemetry backend is reachable.
 *
 * Sends a GET request to the configured health endpoint and expects HTTP 204
 * No Content. This should be called after Wi-Fi is connected.
 *
 * @return ESP_OK if the backend returns the expected health status.
 * @return ESP_FAIL or an HTTP client error if the health check fails.
 */
esp_err_t telemetry_client_check_server(void);

/**
 * @brief Send one telemetry report to the configured backend.
 *
 * Builds a JSON payload containing the configured device ID, monotonic report
 * sequence, uptime in milliseconds, current Wi-Fi RSSI, and the provided DHT11
 * temperature/humidity sample, then posts it to the telemetry endpoint. The
 * sequence number is advanced only after the backend accepts the report with
 * HTTP 200.
 *
 * @param[in] reading Sensor sample to include in the report.
 *
 * @return ESP_OK if the report is accepted.
 * @return ESP_ERR_INVALID_ARG if @p reading is NULL.
 * @return ESP_FAIL or an HTTP client error if building or sending fails.
 */
esp_err_t telemetry_client_send_report(const dht11_reading_t *reading);
