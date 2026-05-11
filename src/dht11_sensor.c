#include "dht11_sensor.h"

#if __has_include("app_config.h")
#include "app_config.h"
#else
#error "Missing app_config.h. Copy include/app_config.example.h to include/app_config.h and fill in your values."
#endif

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_err.h"
#include "esp_log.h"

#define DHT11_RESPONSE_TIMEOUT_US 100
#define DHT11_LOW_TIMEOUT_US 70
#define DHT11_HIGH_TIMEOUT_US 100
#define DHT11_BIT_THRESHOLD_US 40

#define LOW 0
#define HIGH 1

static const char *TAG = "DHT11";

/* Returns how long the line stayed at level, or -1 if it exceeded timeout_us. */
static int wait_while_level(int level, int timeout_us) {
    int elapsed = 0;
    while (gpio_get_level(DHT11_GPIO) == level) {
        if (elapsed >= timeout_us) return -1;
        esp_rom_delay_us(1);
        ++elapsed;
    }
    return elapsed;
}

esp_err_t dht11_sensor_read(dht11_reading_t *reading) {
    if (reading == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    /*
     * Start signal: the host pulls the bus low for at least 18 ms, then
     * releases it high briefly before switching to input mode.
     */
    err = gpio_set_direction(DHT11_GPIO, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO direction");
        return err;
    }

    err = gpio_set_level(DHT11_GPIO, LOW);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO level");
        return err;
    }
    esp_rom_delay_us(18000);

    err = gpio_set_level(DHT11_GPIO, HIGH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO level");
        return err;
    }
    esp_rom_delay_us(30);

    err = gpio_set_direction(DHT11_GPIO, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO direction");
        return err;
    }

    /* Sensor response: low pulse followed by high pulse before data bits. */
    int elapsed = 0;
    elapsed = wait_while_level(LOW, DHT11_RESPONSE_TIMEOUT_US);
    if (elapsed == -1) {
        ESP_LOGE(TAG, "GPIO wait timeout");
        return ESP_ERR_TIMEOUT;
    }
    elapsed = wait_while_level(HIGH, DHT11_RESPONSE_TIMEOUT_US);
    if (elapsed == -1) {
        ESP_LOGE(TAG, "GPIO wait timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t data[5] = {0};
    for(int i = 0; i < 40; ++i) {
        int byte_idx = i/8;
        int bit = 7 - (i%8);

        /* Each bit starts with a low pulse; high pulse duration encodes 0/1. */
        elapsed = wait_while_level(LOW, DHT11_LOW_TIMEOUT_US);
        if (elapsed == -1) {
            ESP_LOGE(TAG, "GPIO wait timeout");
            return ESP_ERR_TIMEOUT;
        }
        elapsed = wait_while_level(HIGH, DHT11_HIGH_TIMEOUT_US);
        if (elapsed == -1) {
            ESP_LOGE(TAG, "GPIO wait timeout");
            return ESP_ERR_TIMEOUT;
        }

        if (elapsed > DHT11_BIT_THRESHOLD_US)
            data[byte_idx] |= 1 << bit;
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGE(TAG, "Checksum error");
        return ESP_ERR_INVALID_CRC;
    }

    reading->temperature_c = data[2] + data[3] / 10.f;
    reading->humidity_percent = data[0] + data[1] / 10.f;

    return ESP_OK;
}
