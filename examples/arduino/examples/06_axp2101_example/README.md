# 06_axp2101_example

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Reads AXP2101 power rails, battery status and interrupt events.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

Use Arduino-ESP32 3.3.10 and **ESP32C5 Dev Module**. Install the bundled [libraries](../../libraries/), apply the board settings in the [repository README](../../../../README.md), upload this directory's `.ino` sketch and open the serial monitor at 115200 baud.

## Expected Result

The serial monitor reports power status and detected events.

## Troubleshooting

Install the bundled XPowersLib. This example does not change or disable power rails.

