# ESP32-C5-Touch-LCD-3.5 BSP

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Provides display, touch, SD card and onboard peripheral interfaces used by this example.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 with an ST7796 LCD and FT6336 touch controller. BOOT uses GPIO28 (active-low); PWR is read through EXIO6 (active-high), rather than an MCU GPIO.

## How to Run

Build and flash using the enclosing [example instructions](../../README.md). Display applications initialize the IO expander before starting the display.

## Expected Result

The enclosing example exercises the board interfaces as described in its README.

## Troubleshooting

LCD and SD share SPI signals with separate chip selects; normal touch input requires FT6336 gesture mode to be disabled.

