# 04_qmi8658

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取 QMI8658 的加速度和角速度。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

移动或旋转板子，串口读数随之变化。

## 常见问题

示例使用板载传感器，无需外接 IMU。

