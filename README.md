# ESP32 Air Quality Monitor

An ESP32-based environmental monitoring prototype developed as part of the Engineering Intern practical task for Aerion Technology Labs Pvt. Ltd.

## Project Overview

This project uses an ESP32-WROOM-32 to generate simulated temperature and humidity readings and transmit them through Bluetooth Low Energy (BLE).

### Implemented Features

- Simulated temperature and humidity readings using an NTP-synchronized time-based model.
- Five-sample moving average filtering.
- BLE communication with Read and Notify characteristics.
- Configurable sampling interval through BLE.
- Temperature and humidity threshold alerts.
- Python-based BLE logger that records readings in a CSV file.

**Note:** No physical temperature or humidity sensor was connected. Both readings are simulated. NTP is used for time synchronization, not for retrieving actual weather data.

## BLE Configuration

**Device Name:** `AQM-Sabesan`

**Service UUID:**
`12345678-1234-1234-1234-1234567890ab`

| Characteristic | UUID | Properties |
|---|---|---|
| Temperature | `12345678-1234-1234-1234-1234567890ac` | Read, Notify |
| Humidity | `12345678-1234-1234-1234-1234567890ad` | Read, Notify |
| Sampling Interval | `12345678-1234-1234-1234-1234567890ae` | Read, Write |
| Alert Status | `12345678-1234-1234-1234-1234567890af` | Read, Notify |

## Project Structure

```text
ESP32-Air-Quality-Monitor/
├── demo/
│   └── nRF_screen_record.mp4
├── firmware/
│   └── AQM_Sabesan_With_Alerts.ino
├── python-logger/
│   └── aqm_ble_data_logger.py
├── screenshots/
│   ├── 01_BLE_Temperature_Humidity_Alert.png
│   └── 02_Sampling_Interval.png.jpeg
└── README.md
```

## Setup and Testing

1. Open the firmware in Arduino IDE.
2. Enter your Wi-Fi credentials in the firmware.
3. Select **ESP32 Dev Module** and upload the sketch.
4. Connect to `AQM-Sabesan` using nRF Connect to inspect readings and test BLE characteristics.
5. Install the Python dependency:

   `pip install bleak`

6. Run `aqm_ble_data_logger.py` to record readings into a CSV file.

## Demo and Screenshots

**Project Demonstration:** [Watch the screen recording](demo/nRF_screen_record.mp4)

The `screenshots/` folder contains evidence of BLE temperature and humidity readings, alert status, and sampling interval configuration.

## AI Usage Disclosure

I used AI tools extensively throughout this task to generate, explain, troubleshoot, and modify the ESP32 firmware and Python logger. My existing knowledge of Arduino C++, ESP32 BLE programming, and Python is limited, so I relied heavily on AI assistance to develop the implementation. I did not independently develop the complete solution using my own programming knowledge.

I uploaded the firmware to the ESP32 and tested the BLE functionality using nRF Connect. I also ran the Python logger and verified that it recorded readings in a CSV file. I am still learning the underlying concepts and may need assistance understanding or modifying certain parts of the implementation.
