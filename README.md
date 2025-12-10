# Gigstack Hardware

ESP8266-based display device that shows financial data from Blynk cloud.

## Features

- OLED display showing revenue amount and date
- RGB LED status indicator
- Temperature/humidity monitoring (DHT11)
- WiFi setup via captive portal
- Factory reset button

## Hardware Requirements

- ESP8266 (NodeMCU v2 or similar)
- SSD1306 OLED display (128x64, I2C)
- RGB LED (4-pin, common cathode)
- DHT11 temperature/humidity sensor
- Push button for reset

## Wiring

### OLED Display (I2C)

| OLED Pin | ESP8266 Pin |
|----------|-------------|
| VCC | 3.3V |
| GND | GND |
| SDA | D2 |
| SCL | D1 |

### RGB LED (Common Cathode)

| LED Pin | ESP8266 Pin |
|---------|-------------|
| Red (pin 1) | D6 |
| Common/GND (pin 2, long leg) | GND |
| Green (pin 3) | D7 |
| Blue (pin 4) | D0 |

### DHT11 Sensor

| DHT11 Pin | ESP8266 Pin |
|-----------|-------------|
| VCC (pin 1, left) | 3.3V |
| DATA (pin 2) | D3 |
| NC (pin 3) | - |
| GND (pin 4, right) | GND |

### Reset Button

| Button | ESP8266 Pin |
|--------|-------------|
| One side | D5 |
| Other side | GND |

## Pin Summary

| Component | ESP8266 Pin | GPIO |
|-----------|-------------|------|
| OLED SDA | D2 | GPIO4 |
| OLED SCL | D1 | GPIO5 |
| Reset Button | D5 | GPIO14 |
| LED Red | D6 | GPIO12 |
| LED Green | D7 | GPIO13 |
| LED Blue | D0 | GPIO16 |
| DHT11 Data | D3 | GPIO0 |

## LED Status Indicators

| Color | Status |
|-------|--------|
| Green (steady) | Connected to Blynk |
| Red (steady) | Disconnected |
| Blue (steady) | WiFi setup mode |

## Setup

1. Flash the firmware using PlatformIO
2. On first boot, connect to WiFi network "gigstack-Setup"
3. Open browser to 192.168.4.1
4. Enter your WiFi credentials and Blynk token
5. Device will restart and connect

## Factory Reset

Hold the reset button for 3 seconds to clear WiFi and Blynk settings.

## Building

```bash
pio run
pio run -t upload
```

## Firmware Version

Current: v2.0
