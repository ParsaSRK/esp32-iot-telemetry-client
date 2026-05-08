# ESP32 IoT Telemetry Client

ESP32 IoT client built with ESP-IDF and FreeRTOS.

Current status:
- Wi-Fi station setup
- WPA2-Enterprise credential support
- connection retry handling
- IP address logging after connection
- credentials kept out of git

Planned:
- HTTP GET/POST support
- telemetry reporting to a backend server
- GPIO or sensor data reporting

## Setup

Create a local Wi-Fi secrets file:

```sh
cp include/wifi_secret.example.h include/wifi_secret.h
```

Edit `include/wifi_secret.h` with your Wi-Fi credentials.

## Build

```sh
pio run
```

## Upload

```sh
pio run -t upload
pio device monitor
```
