# 01_lvgl_demo

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在 ST7796 显示屏运行 LVGL benchmark，初始化 FT6336 触摸输入。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

屏幕循环显示 LVGL 基准测试场景。

## 常见问题

该示例运行基准测试，不显示出厂应用桌面。

