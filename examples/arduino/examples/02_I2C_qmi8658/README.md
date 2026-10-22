# 02_I2C_qmi8658

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Reads QMI8658 acceleration and angular velocity.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

Use Arduino-ESP32 3.3.10 and **ESP32C5 Dev Module**. Install the bundled [libraries](../../libraries/), apply the board settings in the [repository README](../../../../README.md), upload this directory's `.ino` sketch and open the serial monitor at 115200 baud.

## Expected Result

Serial readings change as you move the board.

## Troubleshooting

Use the bundled SensorLib when the sketch requests it.

