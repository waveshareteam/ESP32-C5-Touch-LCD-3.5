# Firmware

[English](README.md) | [简体中文](README_ZH.md)

| File | Purpose |
| --- | --- |
| [ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin](ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin) | 12-application factory desktop with Xiaozhi, SHTC3 temperature/humidity, Button Test and Display Test; ESP-IDF v5.5.4 |

This directory contains only the latest factory firmware. The merged image includes the bootloader, partition table, application and speech model, built for the board's **32 MB Flash and 8 MB PSRAM**. Flash at **0x0**; camera functions require a connected BF3901 module.

```bash
python -m pip install esptool
python -m esptool --chip esp32c5 --port PORT write_flash 0x0 ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin
```

Run from this directory, replacing PORT with your serial port, such as COMx or /dev/ttyACMx. Flashing replaces the firmware; retain any device settings you need beforehand.

SHA256: `5de2731c4a9d5100a538c1927afe3ab11b2252f08a1a073e46bf9e8a3e9fb050`.

[Factory source](../examples/factory/README.md) | [Application user guide](../examples/factory/USER_GUIDE.md)

