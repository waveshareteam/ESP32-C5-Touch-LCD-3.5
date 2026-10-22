# 05_pcf85063

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Sets and reads the onboard PCF85063A RTC.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The serial monitor prints a clock that advances every second.

## Troubleshooting

Each boot sets the RTC to 2026-09-22 15:00:00; this is a fixed demo time.

