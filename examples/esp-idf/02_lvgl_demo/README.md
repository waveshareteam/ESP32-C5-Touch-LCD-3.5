# 02_lvgl_demo

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the LVGL benchmark on the ST7796 display with FT6336 touch input.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The display cycles through LVGL benchmark scenes.

## Troubleshooting

This example starts the benchmark, rather than the factory application desktop.

