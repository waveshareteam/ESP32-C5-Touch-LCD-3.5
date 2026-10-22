# 08_AXP2101

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the AXP2101 PMIC and reports power and interrupt status.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The serial monitor reports PMIC information and detected power events.

## Troubleshooting

Connect a compatible battery only when testing battery-related functions.

