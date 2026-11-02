
import asyncio
import csv
from datetime import datetime, timezone, timedelta
from pathlib import Path

from bleak import BleakClient, BleakScanner

# ESP32 BLE device and service UUIDs
DEVICE_NAME = "AQM-Sabesan"

SERVICE_UUID = "12345678-1234-1234-1234-1234567890ab"

TEMPERATURE_UUID = "12345678-1234-1234-1234-1234567890ac"
HUMIDITY_UUID = "12345678-1234-1234-1234-1234567890ad"
INTERVAL_UUID = "12345678-1234-1234-1234-1234567890ae"
ALERT_UUID = "12345678-1234-1234-1234-1234567890af"

# Save CSV beside this Python script
CSV_FILE = Path(__file__).with_name("aqm_readings.csv")

# Indian Standard Time
IST = timezone(timedelta(hours=5, minutes=30), name="IST")

latest_temperature = ""
latest_humidity = ""
latest_alert = "UNKNOWN"

csv_writer = None
csv_file_handle = None


def decode_value(data: bytearray) -> str:
    return bytes(data).decode("utf-8", errors="replace").strip()


def write_csv_row():
    if not latest_temperature or not latest_humidity:
        return

    timestamp = datetime.now(IST).isoformat(timespec="seconds")

    csv_writer.writerow([
        timestamp,
        latest_temperature,
        latest_humidity,
        latest_alert
    ])

    csv_file_handle.flush()

    print(
        f"{timestamp} | "
        f"Temperature: {latest_temperature} °C | "
        f"Humidity: {latest_humidity} % | "
        f"Alert: {latest_alert}"
    )


def temperature_handler(sender, data):
    global latest_temperature
    latest_temperature = decode_value(data)


def humidity_handler(sender, data):
    global latest_humidity
    latest_humidity = decode_value(data)
    write_csv_row()


def alert_handler(sender, data):
    global latest_alert
    latest_alert = decode_value(data)
    print(f"Alert update: {latest_alert}")


async def find_device():
    print(f"Searching for BLE device: {DEVICE_NAME}")
    print(f"Looking for service UUID: {SERVICE_UUID}")

    devices = await BleakScanner.discover(
        timeout=15.0,
        return_adv=True
    )

    for address, (device, advertisement) in devices.items():

        advertised_name = advertisement.local_name or device.name

        service_uuids = [
            uuid.lower()
            for uuid in (advertisement.service_uuids or [])
        ]

        if SERVICE_UUID.lower() in service_uuids:
            print(f"Found ESP32: {advertised_name}")
            print(f"Bluetooth address: {address}")
            return device

        if (
            advertised_name
            and advertised_name.casefold() == DEVICE_NAME.casefold()
        ):
            print(f"Found ESP32: {advertised_name}")
            print(f"Bluetooth address: {address}")
            return device

    raise RuntimeError(
        "ESP32 was not found. Check that it is powered on "
        "and advertising."
    )


async def main():
    global csv_writer
    global csv_file_handle
    global latest_temperature
    global latest_humidity
    global latest_alert

    print("AQM ESP32 CSV Logger")
    print(f"CSV output: {CSV_FILE}")
    print("Keep the ESP32 powered on and advertising.")
    print("Disconnect nRF Connect before starting this logger.")

    device = await find_device()

    file_exists = CSV_FILE.exists() and CSV_FILE.stat().st_size > 0

    csv_file_handle = CSV_FILE.open(
        "a",
        newline="",
        encoding="utf-8"
    )

    csv_writer = csv.writer(csv_file_handle)

    if not file_exists:
        csv_writer.writerow([
            "Timestamp_IST",
            "Temperature_C",
            "Humidity_Percent",
            "Alert_Status"
        ])
        csv_file_handle.flush()

    try:
        async with BleakClient(device) as client:

            print("Connected to ESP32.")

            # Read the current values
            try:
                value = await client.read_gatt_char(TEMPERATURE_UUID)
                latest_temperature = decode_value(value)

                value = await client.read_gatt_char(HUMIDITY_UUID)
                latest_humidity = decode_value(value)

                value = await client.read_gatt_char(ALERT_UUID)
                latest_alert = decode_value(value)

                print(
                    f"Initial values: {latest_temperature} °C, "
                    f"{latest_humidity} %, alert={latest_alert}"
                )

            except Exception as error:
                print(f"Initial read warning: {error}")

            # Enable BLE notifications
            await client.start_notify(
                TEMPERATURE_UUID,
                temperature_handler
            )

            await client.start_notify(
                HUMIDITY_UUID,
                humidity_handler
            )

            await client.start_notify(
                ALERT_UUID,
                alert_handler
            )

            print("Notifications enabled.")
            print("Logging readings. Press Ctrl+C to stop.")

            while client.is_connected:
                await asyncio.sleep(1)

    except asyncio.CancelledError:
        raise

    except Exception as error:
        print(f"BLE logger stopped: {error}")

    finally:
        if csv_file_handle:
            csv_file_handle.close()
            print(f"CSV saved to: {CSV_FILE}")


if __name__ == "__main__":
    try:
        asyncio.run(main())

    except KeyboardInterrupt:
        print("\nLogger stopped by user.")
