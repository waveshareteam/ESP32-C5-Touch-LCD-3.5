# 03_SD_Card

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Mounts a microSD card and prints its information and capacity.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable. A microSD card is also required.

## How to Run

Use Arduino-ESP32 3.3.10 and **ESP32C5 Dev Module**. Install the bundled [libraries](../../libraries/), apply the board settings in the [repository README](../../../../README.md), upload this directory's `.ino` sketch and open the serial monitor at 115200 baud.

## Expected Result

The serial monitor shows the detected card and its capacity.

## Troubleshooting

The example can format the card if mounting fails; back up its contents first.

