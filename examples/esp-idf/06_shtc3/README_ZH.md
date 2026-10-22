# 06_shtc3

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取板载 SHTC3 温湿度传感器。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

串口输出温度和相对湿度。

## 常见问题

该独立示例与出厂 Sensor 应用不同，后者仅显示 RTC 和 IMU 数据。

