# ESP32 IoT Telemetry Client

ESP32 telemetry firmware built with ESP-IDF, FreeRTOS, PlatformIO, and cJSON.
The device connects to Wi-Fi, reads a DHT11 temperature/humidity sensor, and
sends periodic HTTP JSON reports to a backend server.

## Features

- Wi-Fi station mode with WPA2-Enterprise credential support
- Connection retry handling and IP address logging
- DHT11 temperature/humidity sensor driver
- HTTP health check before reporting telemetry
- JSON telemetry payloads using cJSON
- Local-only configuration for Wi-Fi credentials, DHT11 GPIO, server URL, and
  device ID

## Hardware

Current sensor:

- DHT11 temperature/humidity module, 3-pin version

Wiring:

```text
DHT11 VCC  -> ESP32 3V3
DHT11 GND  -> ESP32 GND
DHT11 DATA -> configured DHT11_GPIO, default GPIO 4
```

The 3-pin DHT11 module usually has a pull-up resistor already installed. If
readings are unstable, add a 10k pull-up resistor between DATA and 3V3.

## Dependencies

Host tools:

- PlatformIO Core
- ESP-IDF through PlatformIO
- USB serial access to the ESP32

PlatformIO project dependencies:

- `espressif32` platform
- `espidf` framework
- `cJSON` from `https://github.com/DaveGamble/cJSON.git`

The dependency is declared in `platformio.ini`.

## Configuration

Runtime configuration is intentionally kept out of git.

Create a local config file:

```sh
cp include/app_config.example.h include/app_config.h
```

Edit `include/app_config.h`:

```c
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_IDENTITY "YOUR_WIFI_IDENTITY"
#define WIFI_USERNAME "YOUR_WIFI_USERNAME"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

#define DHT11_GPIO 4

#define TELEMETRY_BASE_URL "http://YOUR_SERVER_IP_OR_DOMAIN"
#define HEALTH_URL TELEMETRY_BASE_URL "/health"
#define TELEMETRY_URL TELEMETRY_BASE_URL "/telemetry"
#define DEVICE_ID "esp32-dev-1"
```

`include/app_config.h` is ignored by git. Commit only
`include/app_config.example.h`.

## Backend API

The firmware expects a backend with two HTTP endpoints.

Health check:

```text
GET /health
```

Expected response:

```text
204 No Content
```

Telemetry report:

```text
POST /telemetry
Content-Type: application/json
```

Request body:

```json
{
  "device_id": "esp32-dev-1",
  "sequence": 1,
  "uptime_ms": 123456,
  "wifi_rssi": -61,
  "temperature_c": 26.7,
  "humidity_percent": 35.0
}
```

Expected response:

```text
200 OK
```

Current firmware checks only the HTTP status code. The sequence number is
incremented after the backend accepts the report with HTTP 200.

## Device Protocol

On boot, the firmware:

1. Initializes NVS.
2. Connects to Wi-Fi using settings from `app_config.h`.
3. Sends `GET /health` to verify the backend is reachable.
4. Repeats every 5 seconds:
   - reads DHT11 temperature/humidity,
   - gets current Wi-Fi RSSI,
   - builds a JSON telemetry payload,
   - sends it with `POST /telemetry`.

DHT11 readings are decoded from the sensor's single-wire pulse protocol. The
driver validates the checksum before returning a reading.

## Build

```sh
pio run
```

## Upload

```sh
pio run -t upload
```

## Monitor

```sh
pio device monitor
```

The monitor baud rate is configured as `115200` in `platformio.ini`.

## Project Layout

```text
include/
  app_config.example.h   public configuration template
  dht11_sensor.h         DHT11 driver API
  telemetry_client.h     HTTP telemetry API
  wifi_app.h             Wi-Fi connection API

src/
  main.c                 application entry point
  dht11_sensor.c         DHT11 protocol implementation
  telemetry_client.c     health check and telemetry POST client
  wifi_app.c             Wi-Fi station/WPA2-Enterprise setup
```

## Notes

- ESP32 GPIO is not 5V tolerant. Power the DHT11 module from 3V3.
- `include/app_config.h`, `.pio`, `compile_commands.json`, and generated
  SDK config files are ignored by git.
- The current board configuration targets `esp32dev` with 2MB flash.
