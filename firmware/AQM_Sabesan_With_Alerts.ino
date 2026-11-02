#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// WI-FI SETTINGS
// Enter your phone hotspot details exactly as shown on your phone.
// ESP32-WROOM-32 supports 2.4 GHz Wi-Fi, not 5 GHz Wi-Fi.
// ============================================================

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// India Standard Time (IST) = UTC + 5 hours 30 minutes.
const long GMT_OFFSET_SECONDS = 19800;
const int DAYLIGHT_OFFSET_SECONDS = 0;

// ============================================================
// BLE SETTINGS -- KEEP THESE UUIDs UNCHANGED FOR YOUR APP
// ============================================================

#define DEVICE_NAME "AQM-Sabesan"

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define TEMPERATURE_UUID    "12345678-1234-1234-1234-1234567890ac"
#define HUMIDITY_UUID       "12345678-1234-1234-1234-1234567890ad"
#define INTERVAL_UUID       "12345678-1234-1234-1234-1234567890ae"
#define ALERT_UUID          "12345678-1234-1234-1234-1234567890af"

// Alert thresholds are evaluated using the five-sample moving averages.
const float HIGH_TEMPERATURE_C = 30.0f;
const float HIGH_HUMIDITY_PERCENT = 85.0f;

// ============================================================
// SAMPLING AND SIMULATION SETTINGS
// ============================================================

const int WINDOW_SIZE = 5;
const unsigned long NTP_CHECK_INTERVAL_MS = 1000UL;

volatile unsigned long samplingIntervalMs = 2000UL;
volatile bool intervalChanged = false;

float temperatureSamples[WINDOW_SIZE] = {0};
float humiditySamples[WINDOW_SIZE] = {0};

int sampleIndex = 0;
int samplesCollected = 0;

unsigned long previousSampleTime = 0;

// Wi-Fi and clock state. The ESP32 driver handles automatic reconnects.
volatile bool wifiGotIpEventPending = false;
volatile bool wifiDisconnectEventPending = false;
volatile uint8_t lastWiFiDisconnectReason = 0;

bool wifiConnectedMessagePrinted = false;
bool timeIsReady = false;
bool ntpConfigStarted = false;
unsigned long lastNtpCheckAt = 0;

// ============================================================
// BLE CHARACTERISTIC POINTERS
// ============================================================

BLECharacteristic *temperatureCharacteristic;
BLECharacteristic *humidityCharacteristic;
BLECharacteristic *intervalCharacteristic;
BLECharacteristic *alertCharacteristic;

// These are string literals; the state changes only when a threshold status changes.
const char *currentAlertState = "WAITING";
bool alertStateInitialized = false;

// ============================================================
// WIFI STARTUP AND DIAGNOSTICS
// ============================================================

void printNearbyNetworks() {
  Serial.println();
  Serial.println("Scanning for nearby Wi-Fi networks...");
  Serial.print("Configured SSID: [");
  Serial.print(WIFI_SSID);
  Serial.print("], length: ");
  Serial.println(strlen(WIFI_SSID));

  int networkCount = WiFi.scanNetworks();
  bool targetFound = false;

  if (networkCount < 0) {
    Serial.print("Wi-Fi scan failed. Scan result: ");
    Serial.println(networkCount);
    return;
  }

  if (networkCount == 0) {
    Serial.println("No Wi-Fi networks found.");
  } else {
    for (int i = 0; i < networkCount; i++) {
      String foundSSID = WiFi.SSID(i);

      Serial.print("  SSID: [");
      Serial.print(foundSSID);
      Serial.print("], length: ");
      Serial.print(foundSSID.length());
      Serial.print(", RSSI: ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm, security mode: ");
      Serial.println((int)WiFi.encryptionType(i));

      // strcmp performs an exact byte-for-byte comparison.
      if (strcmp(foundSSID.c_str(), WIFI_SSID) == 0) {
        targetFound = true;
        Serial.print("Configured hotspot found. Signal: ");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm");
      }
    }
  }

  if (!targetFound) {
    Serial.println("The configured SSID did not exactly match a scanned SSID.");
    Serial.println("Compare the SSID text and lengths printed above.");
    Serial.println("The ESP32 will still attempt the configured SSID.");
  }

  WiFi.scanDelete();
  Serial.println();
}

// This callback runs in the Wi-Fi event task. It only stores event data;
// Serial output is handled later from loop().
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiGotIpEventPending = true;
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastWiFiDisconnectReason = info.wifi_sta_disconnected.reason;
    wifiDisconnectEventPending = true;
  }
}

void startWiFi() {
  Serial.println();
  Serial.print("Preparing Wi-Fi for hotspot: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWiFiEvent);
  delay(300);

  printNearbyNetworks();

  Serial.println("Starting one Wi-Fi connection request.");
  Serial.println("The ESP32 Wi-Fi driver will handle retries automatically.");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void serviceWiFiConnection() {
  if (wifiGotIpEventPending) {
    wifiGotIpEventPending = false;
    wifiConnectedMessagePrinted = true;
    Serial.println();
    Serial.println("Wi-Fi connected successfully.");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
  }

  if (wifiDisconnectEventPending) {
    uint8_t reason = lastWiFiDisconnectReason;
    wifiDisconnectEventPending = false;
    wifiConnectedMessagePrinted = false;

    Serial.print("Wi-Fi disconnected. ESP32 reason code: ");
    Serial.println(reason);

    switch (reason) {
      case 2:
        Serial.println("Reason: authentication expired.");
        break;
      case 15:
        Serial.println("Reason: WPA handshake timed out. Check hotspot password/security compatibility.");
        break;
      case 201:
        Serial.println("Reason: access point not found during connection attempt.");
        break;
      case 202:
        Serial.println("Reason: authentication failed. Check password and hotspot security mode.");
        break;
      case 203:
        Serial.println("Reason: association with the hotspot failed.");
        break;
      case 204:
        Serial.println("Reason: connection handshake timed out. Try WPA2-Personal/compatibility mode.");
        break;
      default:
        Serial.println("Reason code is from the ESP32 Wi-Fi driver; share it for diagnosis.");
        break;
    }
  }

  // Fallback in case the GOT_IP event was processed before this loop check.
  if (WiFi.status() == WL_CONNECTED && !wifiConnectedMessagePrinted) {
    wifiConnectedMessagePrinted = true;
    Serial.println("Wi-Fi connected successfully.");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
  }
}

// ============================================================
// NTP CLOCK SYNCHRONISATION
// ============================================================

void serviceNTPClock() {
  if (WiFi.status() != WL_CONNECTED || timeIsReady) {
    return;
  }

  // Configure the SNTP client once after Wi-Fi obtains an IP address.
  if (!ntpConfigStarted) {
    Serial.println("Requesting current time from NTP...");
    configTime(
      GMT_OFFSET_SECONDS,
      DAYLIGHT_OFFSET_SECONDS,
      "pool.ntp.org",
      "time.google.com",
      "time.cloudflare.com"
    );
    ntpConfigStarted = true;
    lastNtpCheckAt = millis();
  }

  if (millis() - lastNtpCheckAt < NTP_CHECK_INTERVAL_MS) {
    return;
  }
  lastNtpCheckAt = millis();

  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 10) && timeInfo.tm_year >= (2024 - 1900)) {
    timeIsReady = true;
    previousSampleTime = millis();

    Serial.println("Clock synchronised successfully.");
    Serial.print("Current IST time: ");
    Serial.println(&timeInfo, "%d-%m-%Y %I:%M:%S %p");
    Serial.println("Time-based simulated readings are starting.");
  }
}

// ============================================================
// TIME-BASED SIMULATED TEMPERATURE AND HUMIDITY
// These are illustrative simulated values, not physical sensor data
// or a live weather forecast.
// ============================================================

void generateSimulatedReadings(float &temperature, float &humidity) {
  struct tm timeInfo;

  if (!getLocalTime(&timeInfo, 10) || timeInfo.tm_year < (2024 - 1900)) {
    temperature = NAN;
    humidity = NAN;
    return;
  }

  float hourOfDay = timeInfo.tm_hour
                  + timeInfo.tm_min / 60.0f
                  + timeInfo.tm_sec / 3600.0f;

  const float PI_VALUE = 3.14159265f;

  // Smooth daily cycle: warmer in the afternoon, cooler overnight.
  float dailyCycle = cosf(
    2.0f * PI_VALUE * (hourOfDay - 14.5f) / 24.0f
  );

  temperature = 26.5f + 5.5f * dailyCycle;
  humidity = 75.0f - 15.0f * dailyCycle;

  // Add small variation so successive readings are not identical.
  temperature += random(-20, 21) / 100.0f;
  humidity += random(-40, 41) / 100.0f;

  temperature = constrain(temperature, 21.0f, 32.0f);
  humidity = constrain(humidity, 60.0f, 90.0f);
}

// ============================================================
// FIVE-SAMPLE MOVING AVERAGE
// ============================================================

void calculateMovingAverage(float &avgTemp, float &avgHumidity) {
  float tempSum = 0.0f;
  float humiditySum = 0.0f;

  for (int i = 0; i < samplesCollected; i++) {
    tempSum += temperatureSamples[i];
    humiditySum += humiditySamples[i];
  }

  avgTemp = tempSum / samplesCollected;
  avgHumidity = humiditySum / samplesCollected;
}

// ============================================================
// ALERT STATUS
// Notify the phone only when the alert state changes.
// Possible values: NORMAL, TEMP_HIGH, HUMIDITY_HIGH, BOTH_HIGH.
// ============================================================

void updateAlertStatus(float averageTemperature, float averageHumidity) {
  bool temperatureHigh = averageTemperature > HIGH_TEMPERATURE_C;
  bool humidityHigh = averageHumidity > HIGH_HUMIDITY_PERCENT;

  const char *newAlertState;

  if (temperatureHigh && humidityHigh) {
    newAlertState = "BOTH_HIGH";
  } else if (temperatureHigh) {
    newAlertState = "TEMP_HIGH";
  } else if (humidityHigh) {
    newAlertState = "HUMIDITY_HIGH";
  } else {
    newAlertState = "NORMAL";
  }

  // Avoid sending the same notification on every sampling cycle.
  if (!alertStateInitialized || strcmp(newAlertState, currentAlertState) != 0) {
    currentAlertState = newAlertState;
    alertStateInitialized = true;

    alertCharacteristic->setValue(currentAlertState);
    alertCharacteristic->notify();

    Serial.print("Alert state changed: ");
    Serial.println(currentAlertState);
  }
}

// ============================================================
// BLE SAMPLING INTERVAL WRITE CALLBACK
// Accepts an integer from 1 to 60 seconds.
// ============================================================

class SamplingIntervalCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String value = characteristic->getValue();

    if (value.length() == 0) {
      Serial.println("Empty sampling interval received.");
      return;
    }

    char *endPointer;
    long seconds = strtol(value.c_str(), &endPointer, 10);

    if (endPointer == value.c_str() || *endPointer != '\0' ||
        seconds < 1 || seconds > 60) {
      Serial.println("Invalid interval. Enter a whole number from 1 to 60.");
      return;
    }

    samplingIntervalMs = (unsigned long)seconds * 1000UL;
    intervalChanged = true;

    char intervalText[8];
    snprintf(intervalText, sizeof(intervalText), "%ld", seconds);
    characteristic->setValue(intervalText);

    Serial.print("New sampling interval: ");
    Serial.print(seconds);
    Serial.println(" seconds");
  }
};

// ============================================================
// BLE SERVICE SETUP
// ============================================================

void setupBLE() {
  BLEDevice::init(DEVICE_NAME);

  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService(SERVICE_UUID);

  // Temperature characteristic: READ + NOTIFY.
  temperatureCharacteristic = service->createCharacteristic(
    TEMPERATURE_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  temperatureCharacteristic->addDescriptor(new BLE2902());
  temperatureCharacteristic->setValue("Waiting for time");

  // Humidity characteristic: READ + NOTIFY.
  humidityCharacteristic = service->createCharacteristic(
    HUMIDITY_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  humidityCharacteristic->addDescriptor(new BLE2902());
  humidityCharacteristic->setValue("Waiting for time");

  // Sampling interval characteristic: READ + WRITE.
  intervalCharacteristic = service->createCharacteristic(
    INTERVAL_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  intervalCharacteristic->setValue("2");
  intervalCharacteristic->setCallbacks(new SamplingIntervalCallback());

  // Alert status: READ + NOTIFY.
  alertCharacteristic = service->createCharacteristic(
    ALERT_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  alertCharacteristic->addDescriptor(new BLE2902());
  alertCharacteristic->setValue("WAITING");

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("BLE service started.");
  Serial.print("Device name: ");
  Serial.println(DEVICE_NAME);
  Serial.println("Waiting for BLE connection...");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  randomSeed((unsigned long)micros());

  Serial.println();
  Serial.println("Starting Air Quality Monitor...");
  Serial.print("High temperature alert threshold: ");
  Serial.print(HIGH_TEMPERATURE_C, 1);
  Serial.println(" C");
  Serial.print("High humidity alert threshold: ");
  Serial.print(HIGH_HUMIDITY_PERCENT, 1);
  Serial.println(" %");

  // BLE starts even when Wi-Fi is unavailable.
  setupBLE();

  // Start Wi-Fi once. The ESP32 driver manages association retries.
  startWiFi();
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  serviceWiFiConnection();
  serviceNTPClock();

  // Apply a new sampling interval written from the phone.
  if (intervalChanged) {
    previousSampleTime = millis();
    intervalChanged = false;
  }

  // Do not create time-based readings until NTP has supplied valid time.
  if (!timeIsReady) {
    delay(10);
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - previousSampleTime >= samplingIntervalMs) {
    previousSampleTime = currentTime;

    float temperature;
    float humidity;
    generateSimulatedReadings(temperature, humidity);

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Valid clock is temporarily unavailable; waiting for NTP.");
      timeIsReady = false;
      return;
    }

    // Add the new readings to the five-position circular buffer.
    temperatureSamples[sampleIndex] = temperature;
    humiditySamples[sampleIndex] = humidity;

    sampleIndex = (sampleIndex + 1) % WINDOW_SIZE;

    if (samplesCollected < WINDOW_SIZE) {
      samplesCollected++;
    }

    float averageTemperature;
    float averageHumidity;
    calculateMovingAverage(averageTemperature, averageHumidity);

    // Check thresholds using filtered values, not individual noisy samples.
    updateAlertStatus(averageTemperature, averageHumidity);

    // BLE transmits the moving averages, as in the previous working sketch.
    char temperatureText[12];
    char humidityText[12];

    snprintf(temperatureText, sizeof(temperatureText), "%.2f", averageTemperature);
    snprintf(humidityText, sizeof(humidityText), "%.2f", averageHumidity);

    temperatureCharacteristic->setValue(temperatureText);
    humidityCharacteristic->setValue(humidityText);

    temperatureCharacteristic->notify();
    humidityCharacteristic->notify();

    // Print raw values, averages, time and active sampling interval.
    struct tm timeInfo;
    Serial.print("IST: ");

    if (getLocalTime(&timeInfo, 10)) {
      Serial.println(&timeInfo, "%I:%M:%S %p");
    } else {
      Serial.println("Unavailable");
    }

    Serial.print("Temperature: ");
    Serial.print(temperature, 2);
    Serial.print(" C | Average: ");
    Serial.print(averageTemperature, 2);
    Serial.print(" C | Humidity: ");
    Serial.print(humidity, 2);
    Serial.print(" % | Average: ");
    Serial.print(averageHumidity, 2);
    Serial.print(" % | Samples: ");
    Serial.print(samplesCollected);
    Serial.print(" | Interval: ");
    Serial.print(samplingIntervalMs / 1000UL);
    Serial.println(" seconds");
  }

  delay(2);
}
