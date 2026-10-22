# 02_sd_card

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Mounts a microSD card and verifies data written to /test.txt.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 and a USB data cable. A microSD card is also required.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run:

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## Expected Result

The serial log reports TF CARD TEST PASS after matching the data.

## Troubleshooting

Use a FAT32 card. This test replaces /test.txt; back up existing files first.

