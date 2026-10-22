# 07_exio

[English](README.md) | [简体中文](README_ZH.md)

## 功能

循环切换 CH32V006 的 EXIO8 至 EXIO15 输出。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

输出电平每秒切换一次，串口同步显示状态。

## 常见问题

EXIO0 至 EXIO7 用于板载资源，不参与本测试。

