#pragma once

#include "esp_err.h"

/**
 * @brief Temperature and humidity sample returned by the DHT11 sensor.
 */
typedef struct {
    /** Temperature in degrees Celsius. */
    float temperature_c;

    /** Relative humidity percentage. */
    float humidity_percent;
} dht11_reading_t;

/**
 * @brief Read one temperature/humidity sample from the DHT11 sensor.
 *
 * The driver performs the complete single-wire DHT11 transaction on the
 * configured GPIO pin, validates the checksum, and fills @p reading only when
 * a complete valid frame is received.
 *
 * @param[out] reading Destination for the decoded sensor reading.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_INVALID_ARG if @p reading is NULL.
 * @return ESP_ERR_TIMEOUT if the sensor does not respond or a pulse times out.
 * @return ESP_ERR_INVALID_CRC if the received checksum is invalid.
 */
esp_err_t dht11_sensor_read(dht11_reading_t *reading);
