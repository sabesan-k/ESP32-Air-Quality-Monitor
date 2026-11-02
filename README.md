# ESP32 Air Quality Monitor with BLE

An ESP32-based air-monitoring prototype developed as a practical assessment for the **Engineering Intern – Air Quality Monitor** position at **Aerion Technology Labs Pvt. Ltd.**

The project demonstrates time-based simulated temperature and humidity readings, five-sample moving-average filtering, Bluetooth Low Energy (BLE) communication, a configurable sampling interval, threshold alerts, and Python-based CSV logging.

> **Measurement note:** This implementation does not use a physical I2C sensor. Both temperature and humidity are simulated in firmware. The ESP32 obtains the current time through NTP and uses the time of day as an input to a mathematical daily-cycle model. The generated values are illustrative, not actual sensor measurements or live weather data.

## 1. Selected Track and Hardware

- **Selected track:** Track A – Physical ESP32 board
- **Board:** ESP32-WROOM-32 development board
- **Development framework:** Arduino framework
- **Development environment:** Arduino IDE
- **BLE test client:** nRF Connect for Android
- **CSV logger environment:** Python 3 on Windows, using Bleak

No external temperature or humidity sensor is connected. Consequently, there is no I2C sensor wiring in this version.

## 2. Features

- Simulated temperature and relative-humidity readings.
- NTP time synchronization over Wi-Fi.
- IST (UTC+05:30) time-based daily simulation.
- Small random variations in generated readings.
- Five-sample moving-average filtering.
- Default sampling interval of 2 seconds.
- BLE sampling-interval write support (1–60 seconds).
- Custom BLE GATT service.
- Temperature and humidity Read + Notify characteristics.
- Sampling interval Read + Write characteristic.
- Alert Status Read + Notify characteristic.
- Python BLE client that records readings to CSV.

## 3. How the Simulation Works

The ESP32 connects to a Wi-Fi hotspot and requests the current time from NTP servers. The firmware applies the IST offset (UTC+05:30) and uses the current hour, minute, and second to calculate a smooth daily cycle.

The core model is:

- `dailyCycle = cos(2π × (hourOfDay − 14.5) / 24)`
- `temperature = 26.5 + 5.5 × dailyCycle`
- `humidity = 75 − 15 × dailyCycle`

Small random variations are then added to both values. The results are constrained to these illustrative ranges:

- Temperature: 21–32 °C
- Relative humidity: 60–90%

This model makes the simulated temperature warmer around the afternoon and cooler overnight, with humidity moving in the opposite direction. It does **not** fetch weather data from an API and should not be treated as a weather forecast.

BLE starts independently of Wi-Fi. Time-based readings begin after the ESP32 has successfully synchronized its clock through NTP.

## 4. Sampling and Filtering

The default sampling interval is **2 seconds**. Each generated temperature and humidity sample is stored in a five-position circular buffer.

The moving average is calculated from the samples currently collected:

`Moving average = Sum of collected samples / Number of collected samples`

The buffer fills until it contains five samples. After that, each new sample replaces the oldest sample. The filtered temperature and humidity averages are the values published through the BLE characteristics.

The sampling interval can be changed from the BLE client by writing a whole-number value from **1 to 60 seconds** to the Sampling Interval characteristic.

## 5. BLE GATT Configuration

**Advertised device name:** `AQM-Sabesan`

**Custom service UUID:**

`12345678-1234-1234-1234-1234567890ab`

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| Temperature | `12345678-1234-1234-1234-1234567890ac` | Read, Notify | Filtered temperature in °C |
| Humidity | `12345678-1234-1234-1234-1234567890ad` | Read, Notify | Filtered relative humidity in % |
| Sampling Interval | `12345678-1234-1234-1234-1234567890ae` | Read, Write | Sampling interval in seconds |
| Alert Status | `12345678-1234-1234-1234-1234567890af` | Read, Notify | Alert state as text |

The values are sent as readable text strings. The temperature and humidity characteristics notify connected clients when a new filtered reading is produced. The alert characteristic notifies when the alert state changes.

## 6. Alert Logic

The firmware checks the five-sample moving averages against these thresholds:

- Temperature greater than 30.0 °C: `TEMP_HIGH`
- Humidity greater than 85.0%: `HUMIDITY_HIGH`
- Both thresholds exceeded: `BOTH_HIGH`
- Neither threshold exceeded: `NORMAL`

The alert status is available through the Alert Status characteristic. A notification is sent when the status changes, rather than repeatedly sending the same status on every sample.

## 7. Hardware and Wi-Fi Setup

The project uses an ESP32-WROOM-32 development board connected to a computer over USB.

No external sensor wiring is required because both environmental readings are simulated.

Wi-Fi is used for NTP time synchronization. Before compiling the firmware, replace the following placeholders in `firmware/AQM_Sabesan_With_Alerts.ino` with your own 2.4 GHz Wi-Fi hotspot details:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Do not commit real Wi-Fi credentials to a public repository. The ESP32-WROOM-32 supports 2.4 GHz Wi-Fi; configure the hotspot accordingly.

## 8. Build and Run the ESP32 Firmware

1. Install Arduino IDE and the ESP32 board package.
2. Open `firmware/AQM_Sabesan_With_Alerts.ino`.
3. Enter your own Wi-Fi SSID and password as described above.
4. Select **ESP32 Dev Module** (or the matching ESP32-WROOM-32 board option).
5. Select the serial port for the connected ESP32.
6. Compile and upload the sketch.
7. Open Serial Monitor at **115200 baud**.
8. Wait for Wi-Fi connection and successful NTP clock synchronization.
9. Once the clock is synchronized, simulated readings begin at the configured interval.

The Serial Monitor prints the IST time, current generated values, moving averages, number of collected samples, and active sampling interval.

## 9. Test with nRF Connect

1. Power the ESP32 and wait for it to start BLE advertising.
2. Open nRF Connect on an Android phone and scan for `AQM-Sabesan`.
3. Connect to the device and expand the custom service UUID.
4. Read the Temperature and Humidity characteristics.
5. Enable notifications for both characteristics.
6. Write a new whole-number value (for example, `5`) to the Sampling Interval characteristic.
7. Read the interval characteristic again and check the Serial Monitor for the updated interval.
8. Read the Alert Status characteristic and, if desired, enable its notifications.

Some BLE clients display custom characteristics as “Unknown Characteristic” because the UUIDs are application-specific. Match each characteristic using the UUID table in this README.

## 10. Python BLE CSV Logger

The optional Python client uses the [Bleak](https://bleak.readthedocs.io/) library to discover and connect to the ESP32, read the initial values, subscribe to notifications, and log incoming readings.

### Requirements

- Python 3
- Bluetooth Low Energy support on the computer
- Bleak Python package

Install Bleak:

```bash
python -m pip install bleak
```

Run the logger from the repository root:

```bash
python python-logger/aqm_ble_data_logger.py
```

Keep the ESP32 powered on and advertising. Disconnect nRF Connect from the ESP32 before starting the Python logger, because the ESP32 may not accept another BLE connection at the same time.

The script creates `aqm_readings.csv` beside the Python script. The CSV contains:

| Column | Description |
|---|---|
| `Timestamp_IST` | Timestamp in Indian Standard Time |
| `Temperature_C` | Filtered temperature in °C |
| `Humidity_Percent` | Filtered relative humidity in % |
| `Alert_Status` | Current alert state |

Press `Ctrl+C` in the terminal to stop the logger.

## 11. Test Evidence

The `screenshots/` folder contains evidence captured during testing, including:

- Temperature and humidity BLE readings and notifications.
- Sampling interval modification through nRF Connect.
- Alert characteristic value.
- Serial Monitor output showing readings and the configured interval.

The Python logger was also run against the ESP32 and produced a CSV file containing timestamped temperature, humidity, and alert-status records.

## 12. Known Limitations

- Temperature and humidity are simulated; no physical I2C sensor is connected or read.
- The values are illustrative outputs from a time-based mathematical model, not measured environmental conditions or live weather data.
- Time-based readings require successful Wi-Fi connection and NTP synchronization.
- The prototype does not include a gas, particulate-matter, or other dedicated air-quality sensor.
- The Python logger records notifications; sampling interval changes are demonstrated through the BLE client (nRF Connect).

## 13. AI Usage Note

AI coding assistants, including ChatGPT, were used to assist with firmware development, BLE GATT implementation, the time-based simulation, debugging, and the Python CSV logger. The generated code was compiled and tested iteratively using Arduino IDE, the ESP32 Serial Monitor, nRF Connect, and the Python BLE client. Issues encountered during development were investigated and corrected through testing. AI assistance was used transparently; the author is continuing to build a deeper understanding of the embedded programming and BLE concepts in the implementation.

## 14. Repository Structure

```text
AQM-Sabesan/
├── firmware/
│   └── AQM_Sabesan_With_Alerts.ino
├── python-logger/
│   └── aqm_ble_data_logger.py
├── screenshots/
│   ├── 01_BLE_Temperature_Humidity.png
│   ├── 02_Sampling_Interval.png
│   ├── 03_BLE_Alert.png
│   └── 04_Serial_Monitor.png
└── README.md
```

---

**Project:** ESP32 Air Quality Monitor with BLE  
**Assessment:** Engineering Intern – Air Quality Monitor  
**Organization:** Aerion Technology Labs Pvt. Ltd.  
**Author:** Sabesan K
