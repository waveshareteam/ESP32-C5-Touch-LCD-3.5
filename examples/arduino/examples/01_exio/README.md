# 01_exio

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Toggles CH32V006 outputs EXIO8 through EXIO15.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable.

## How to Run

Use Arduino-ESP32 3.3.10 and **ESP32C5 Dev Module**. Install the bundled [libraries](../../libraries/), apply the board settings in the [repository README](../../../../README.md), upload this directory's `.ino` sketch and open the serial monitor at 115200 baud.

## Expected Result

Serial output alternates between HIGH and LOW once per second.

## Troubleshooting

EXIO0 through EXIO7 remain inputs in this test.

