# 07_AXP2101

[English](README.md) | [简体中文](README_ZH.md)

## 功能

初始化 AXP2101 电源管理芯片，输出电源及中断状态。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

串口输出 PMIC 信息和检测到的电源事件。

## 常见问题

测试电池相关功能时才需要连接规格匹配的电池。

