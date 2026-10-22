# 07_exio

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Toggles CH32V006 expansion outputs EXIO8 through EXIO15.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The output levels toggle once per second and the serial log reports the state.

## Troubleshooting

EXIO0 through EXIO7 are excluded because they control onboard resources.

