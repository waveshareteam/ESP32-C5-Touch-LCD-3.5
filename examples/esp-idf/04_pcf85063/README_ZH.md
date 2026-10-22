# 04_pcf85063

[English](README.md) | [简体中文](README_ZH.md)

## 功能

设置并读取板载 PCF85063A RTC。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

串口每秒输出一次递增的时间。

## 常见问题

每次启动会将 RTC 设置为固定演示时间 2026-09-22 15:00:00。

