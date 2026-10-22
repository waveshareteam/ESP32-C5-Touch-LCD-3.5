# ESP32-C5-Touch-LCD-3.5 Factory Applications

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Provides a 320 × 480 portrait desktop with 12 applications for touch, display, camera, music, recording, networking and Xiaozhi conversation. See the [user guide](USER_GUIDE.md) for controls.

## Hardware Required

ESP32-C5-Touch-LCD-3.5 with 32 MB Flash and 8 MB PSRAM, and a USB data cable. Photos, music and recording files use a FAT32 microSD card; connect a speaker for audio output.

## How to Run

In an ESP-IDF v5.5.4 shell, enter this directory and run the commands below, or use the [merged factory image](../../firmware/README.md).

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

To merge the complete firmware:

```bash
idf.py merge-bin -o ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin -f raw
```

## Expected Result

After startup, the backlight turns on and the three-page desktop appears; swipe left or right to change pages. Press PWR once to power on and hold it for 5 seconds to power off; double-tap in Display Test to return to the desktop.

## Troubleshooting

For initial installation, flash the partition table and speech model as well as the application. Weather and AIChats need internet access over Wi-Fi; camera analysis also requires a server-provided vision service.

