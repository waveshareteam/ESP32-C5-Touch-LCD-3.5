<div align="center">
  <h1>ESP32-C5-Touch-LCD-3.5</h1>
  <p><strong>3.5-inch touch display board with dual-band Wi-Fi 6, camera and audio</strong></p>
  <p><strong>English</strong> | <a href="README_ZH.md">简体中文</a></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-C5-Touch-LCD-3.5"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-C5-Touch-LCD-3.5"></a>
  </p>
  <p>
    <a href="https://www.waveshare.com/esp32-c5-touch-lcd-3.5.htm">🌐 Product</a> &middot;
    <a href="firmware/">📦 Firmware</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/arduino/examples/">🔧 Arduino</a> &middot;
    <a href="hardware/">📚 Documentation</a>
  </p>
  <p><img src="docs/images/ESP32-C5-Touch-LCD-3.5.png" alt="ESP32-C5-Touch-LCD-3.5" width="500"></p>
</div>

---

## ✨ Overview

ESP32-C5-Touch-LCD-3.5 combines a 320 × 480 touch display, camera interface, audio and sensors for interactive and IoT applications. This repository provides Arduino and ESP-IDF examples, factory applications and the schematic.

| Feature | Device / interface |
| --- | --- |
| MCU | ESP32-C5, 2.4 / 5 GHz Wi-Fi 6, Bluetooth LE |
| Memory | 32 MB Flash, 8 MB PSRAM |
| Display | 3.5-inch, 320 × 480, ST7796, SPI |
| Touch | FT6336, I2C |
| Camera | BF3901 (optional module) |
| Audio | ES8311 |
| Sensors | QMI8658, PCF85063A, SHTC3 |
| Power / IO expansion | AXP2101 / CH32V006 |
| Removable storage | microSD, SPI |

## 📦 Quick Start

To restore the factory applications, download the merged image from [firmware](firmware/) and follow the [flashing instructions](firmware/README.md) to flash it at **0x0**. The desktop contains 12 applications; press PWR once to power on and hold it for 5 seconds to power off. See the [factory user guide](examples/factory/USER_GUIDE.md) for application controls.

For individual examples, download the matching `*-combined.zip` when available in the [latest release](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/releases/latest), extract it and run `flash_combined.bat COMx` (Windows) or `./flash_combined.sh /dev/ttyACMx` (Linux). You can also build from source using the example instructions below.

## 🧪 Examples

The [factory project](examples/factory/) provides Settings, AIChats, Touch Test, Photos, Weather, Music, Sensor, 2048, Camera, Mic Test, Button Test and Display Test.

### ESP-IDF

| Example | Main feature |
| --- | --- |
| [01_lvgl_demo](examples/esp-idf/01_lvgl_demo/) | LVGL benchmark and touch display |
| [02_sd_card](examples/esp-idf/02_sd_card/) | microSD read/write verification |
| [03_qmi8658](examples/esp-idf/03_qmi8658/) | QMI8658 acceleration and angular velocity |
| [04_pcf85063](examples/esp-idf/04_pcf85063/) | PCF85063A RTC clock |
| [05_shtc3](examples/esp-idf/05_shtc3/) | SHTC3 temperature and humidity |
| [06_exio](examples/esp-idf/06_exio/) | CH32V006 IO expansion |
| [07_AXP2101](examples/esp-idf/07_AXP2101/) | AXP2101 power management |

### Arduino

| Example | Main feature |
| --- | --- |
| [01_exio](examples/arduino/examples/01_exio/) | CH32V006 IO expansion |
| [02_I2C_qmi8658](examples/arduino/examples/02_I2C_qmi8658/) | QMI8658 acceleration and angular velocity |
| [03_SD_Card](examples/arduino/examples/03_SD_Card/) | microSD card mounting and capacity |
| [04_I2C_pcf85063](examples/arduino/examples/04_I2C_pcf85063/) | PCF85063A RTC clock |
| [05_shtc3](examples/arduino/examples/05_shtc3/) | SHTC3 temperature and humidity |
| [06_axp2101_example](examples/arduino/examples/06_axp2101_example/) | AXP2101 power and battery status |
| [07_lvgl_demo](examples/arduino/examples/07_lvgl_demo/) | LVGL display, touch and brightness controls |

Sketch libraries are bundled in [examples/arduino/libraries](examples/arduino/libraries/).

## 🛠️ Development Environment

| Framework | Version | Examples |
| --- | --- | ---: |
| ESP-IDF | v5.5.4 | 7 |
| Arduino-ESP32 | 3.3.10 | 7 |
| Factory / ESP-IDF | v5.5.4 | 1 |

In Arduino, select **ESP32C5 Dev Module**, enable PSRAM and USB CDC On Boot, and select QIO flash mode. This core's C5 menu offers up to 16 MB: select 16 MB and the `16M Flash (3MB APP/9.9MB FATFS)` partition, using part of the board's 32 MB Flash. The factory ESP-IDF project is configured for 32 MB Flash.

## 📚 Resources

- [Schematic](hardware/schematic/)
- [Product page](https://www.waveshare.net/shop/ESP32-C5-Touch-LCD-3.5.htm)
- [Wiki](https://docs.waveshare.com/ESP32-C5-Touch-LCD-3.5)

## 🤝 Support

For product, hardware and order questions, contact [Waveshare support](https://service.waveshare.com/).

For example or repository problems, [open an issue](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/issues/new/choose).

## 📄 License

Apache License 2.0. See [LICENSE](LICENSE). Bundled third-party libraries and components retain their own licenses.

