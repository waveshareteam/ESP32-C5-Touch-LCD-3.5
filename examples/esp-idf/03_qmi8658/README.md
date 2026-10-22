# 03_qmi8658

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Reads acceleration and angular velocity from the QMI8658 IMU.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

Serial readings change when you move or rotate the board.

## Troubleshooting

The example uses the onboard sensor; no external IMU is required.

