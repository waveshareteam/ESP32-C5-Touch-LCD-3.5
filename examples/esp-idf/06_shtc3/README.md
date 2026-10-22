# 06_shtc3

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Reads the onboard SHTC3 temperature and humidity sensor.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The serial monitor reports temperature and relative humidity.

## Troubleshooting

This standalone test is separate from the factory Sensor application, which shows RTC and IMU data.

