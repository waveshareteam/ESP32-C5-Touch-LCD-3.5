<div align="center">

# ESP32-C5-Touch-LCD-3.5

**3.5 英寸 ESP32-C5 Wi-Fi 6 触摸屏开发板**

[![许可证](https://img.shields.io/github/license/waveshareteam/ESP32-C5-Touch-LCD-3.5)](./LICENSE)

中文 | [English](./README.md) | [产品页面](https://www.waveshare.net/shop/ESP32-C5-Touch-LCD-3.5.htm) | [使用文档](https://docs.waveshare.net/ESP32-C5-Touch-LCD-3.5) | [出厂固件](./Firmware/) | [ESP-IDF 示例](./example/ESP-IDF-V554/) | [Arduino 示例](./example/Arduino-v3.3.10/example/)

</div>

## 简介

ESP32-C5-Touch-LCD-3.5 是一款基于 ESP32-C5 的 3.5 英寸触摸屏开发板资料包。开发板集成 320 x 480 触摸显示屏、摄像头、音频、传感器、Micro SD 卡、电源管理和 IO 扩展资源。本仓库提供 Arduino 示例、ESP-IDF 示例、出厂固件、依赖库和硬件原理图，方便进行硬件验证和二次开发。

本资料包适合以下场景：

- 验证 LCD、触摸、摄像头、音频、传感器、Micro SD 卡和电源管理功能。
- 使用 Arduino 或 ESP-IDF 开发触摸屏应用。
- 在自定义项目中复用板级驱动、LVGL 示例和 ESP-IDF BSP。
- 恢复出厂演示以及查看硬件原理图。

## 硬件概览

| 项目 | 说明 |
| :--- | :--- |
| 主控 | ESP32-C5，支持 Wi-Fi 6 和 Bluetooth LE |
| 显示屏 | 3.5 英寸 LCD，320 x 480 分辨率 |
| LCD 控制器 | ST7796，SPI 接口 |
| 触摸控制器 | FT6336 电容触摸，I2C 地址 `0x38` |
| IO 扩展 | CH32V006，I2C 地址 `0x24` |
| 电源管理 | AXP2101 PMIC，I2C 地址 `0x34` |
| 传感器 | QMI8658 六轴传感器、PCF85063A RTC、SHTC3 温湿度传感器 |
| 摄像头 | BF3901 摄像头传感器 |
| 音频 | ES8311 音频编解码器，支持麦克风输入和扬声器输出 |
| 存储 | Micro SD 卡槽 |
| UI 框架 | Arduino 示例使用 LVGL v8.4.0；ESP-IDF 示例使用 LVGL v9.5.0 |
| 硬件资料 | [原理图](./schematic/ESP32-C5-Touch-LCD-3.5.pdf) |

> CH32V006 用于 IO 扩展，并管理 LCD/触摸复位、背光和部分外设电源。其控制固件在出厂时已经烧录，正常使用时只需烧录 ESP32-C5 应用程序。

## 支持的开发环境

| 开发框架 | 版本 | 示例数量 |
| :--- | :--- | ---: |
| Arduino-ESP32 | `3.3.10` | 7 |
| ESP-IDF | `v5.5.4` | 8 |

## 目录结构

```text
.
├── example/
│   ├── Arduino-v3.3.10/     # Arduino 示例和依赖库
│   └── ESP-IDF-V554/        # ESP-IDF v5.5.4 示例
├── Firmware/                # 出厂固件
├── schematic/               # 硬件原理图
├── sdcard/                  # 预留的 Micro SD 卡测试资源目录
├── LICENSE
├── README.cn.md
├── README.en.md
└── README.md
```

## Arduino 快速开始

推荐环境：

- Arduino IDE
- `esp32 by Espressif Systems v3.3.10`
- 开发板选择：`ESP32C5 Dev Module`

操作步骤：

1. 在 Arduino IDE 的开发板管理器中安装 `esp32 by Espressif Systems v3.3.10`。
2. 在 `Tools` > `Board` 中选择 `ESP32C5 Dev Module`。
3. 将 `example/Arduino-v3.3.10/libraries/` 内示例需要的依赖库复制到 Arduino 库目录。资料包内包含 `lvgl`、`SensorLib`、`XPowersLib` 和 `C5_BF3901_Camera`。
4. 打开 `example/Arduino-v3.3.10/example/` 下的 `.ino` 示例，编译并烧录。
5. 建议先运行 `01_exio`，确认 CH32V006 IO 扩展通信正常，再测试显示屏和其他外设。

Windows 默认 Arduino 库目录通常为：

```text
C:\Users\<用户名>\Documents\Arduino\libraries
```

### Arduino 示例

| 示例 | 功能 |
| :--- | :--- |
| [01_exio](./example/Arduino-v3.3.10/example/01_exio/) | CH32V006 IO 扩展测试 |
| [02_I2C_qmi8658](./example/Arduino-v3.3.10/example/02_I2C_qmi8658/) | QMI8658 六轴传感器测试 |
| [03_SD_Card](./example/Arduino-v3.3.10/example/03_SD_Card/) | Micro SD 卡读写测试 |
| [04_I2C_pcf85063](./example/Arduino-v3.3.10/example/04_I2C_pcf85063/) | PCF85063A RTC 测试 |
| [05_shtc3](./example/Arduino-v3.3.10/example/05_shtc3/) | SHTC3 温湿度传感器测试 |
| [06_axp2101_example](./example/Arduino-v3.3.10/example/06_axp2101_example/) | AXP2101 电源、电池和中断状态测试 |
| [07_lvgl_demo](./example/Arduino-v3.3.10/example/07_lvgl_demo/) | ST7796 显示、FT6336 触摸、背光和 LVGL 测试 |

## ESP-IDF 快速开始

推荐环境：

- ESP-IDF v5.5.4
- 目标芯片：`esp32c5`

进入任意 ESP-IDF 示例目录后运行：

```powershell
idf.py set-target esp32c5
idf.py build flash monitor
```

### ESP-IDF 示例

| 示例 | 功能 |
| :--- | :--- |
| [01_factory](./example/ESP-IDF-V554/01_factory/) | 显示、触摸、摄像头、音频、传感器等板载资源综合出厂示例 |
| [02_lvgl_demo](./example/ESP-IDF-V554/02_lvgl_demo/) | LVGL 显示、触摸和性能测试 |
| [03_sd_card](./example/ESP-IDF-V554/03_sd_card/) | Micro SD 卡挂载及读写校验 |
| [04_qmi8658](./example/ESP-IDF-V554/04_qmi8658/) | QMI8658 加速度计和陀螺仪测试 |
| [05_pcf85063](./example/ESP-IDF-V554/05_pcf85063/) | PCF85063A RTC 测试 |
| [06_shtc3](./example/ESP-IDF-V554/06_shtc3/) | SHTC3 温湿度传感器测试 |
| [07_exio](./example/ESP-IDF-V554/07_exio/) | CH32V006 IO 扩展输出测试 |
| [08_AXP2101](./example/ESP-IDF-V554/08_AXP2101/) | AXP2101 PMIC 配置及状态测试 |

## Micro SD 卡

当前 Micro SD 卡示例不需要预先放置文件。运行测试前请注意：

- 建议将卡格式化为 FAT32。
- 启动示例前插入 Micro SD 卡。
- 读写示例可能创建或覆盖测试文件，请先备份重要数据。

## 出厂固件

- [下载出厂固件](./Firmware/ESP32-C5-Touch-LCD-3.5-20260911.bin)
- 请按照[产品使用文档](https://docs.waveshare.net/ESP32-C5-Touch-LCD-3.5)中的烧录地址和参数进行操作。

该固件可用于恢复或验证板载综合演示。烧录前请确认串口、目标芯片、Flash 模式和烧录地址。

## 二次开发建议

- 常规功能修改优先从示例业务逻辑或 LVGL 应用层开始；仅在硬件设计发生变化时修改 BSP 引脚和电源配置。
- LCD 与 Micro SD 卡共用部分 SPI 信号，并分别使用独立片选。请按照对应示例初始化和释放共享总线。
- 使用 LCD 复位、触摸复位、背光或板载电源控制前，应先初始化 CH32V006。
- AXP2101 管理多路板载电源。未确认负载和启动依赖前，请勿随意关闭电源轨。
- `01_factory` 集成了摄像头、音频、传感器、显示、触摸和 Micro SD 功能；排查单个外设时，建议先运行对应的独立示例。

## 许可证

本仓库采用 Apache License 2.0，详情请查看 [LICENSE](./LICENSE)。

仓库中包含的第三方库和组件仍分别遵循其自身许可证，请同时查看对应目录内的许可证文件。

## 技术支持

产品配置、开发说明和故障排查请查看[微雪文档](https://docs.waveshare.net/ESP32-C5-Touch-LCD-3.5)。反馈问题时，请提供示例路径、开发框架版本、复现步骤和完整串口日志。

- [提交 Issue](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/issues/new)
- [微雪产品文档](https://docs.waveshare.net/ESP32-C5-Touch-LCD-3.5)
