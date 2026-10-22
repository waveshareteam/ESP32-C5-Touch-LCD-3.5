# 07_lvgl_demo

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Tests the ST7796 display, FT6336 touch and backlight using LVGL.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

Use Arduino-ESP32 3.3.10 and **ESP32C5 Dev Module**. Install the bundled [libraries](../../libraries/), apply the board settings in the [repository README](../../../../README.md), upload this directory's `.ino` sketch and open the serial monitor at 115200 baud.

## Expected Result

The display responds to the on-screen controls and brightness changes.

## Troubleshooting

Use the bundled LVGL 8.4 library and enable PSRAM; LVGL 9 is incompatible with this sketch.

