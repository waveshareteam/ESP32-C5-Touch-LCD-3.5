# 05_shtc3

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取 SHTC3 温湿度并校验 CRC。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。

## 怎么运行

使用 Arduino-ESP32 3.3.10，选择 **ESP32C5 Dev Module**，将仓库随附的 [libraries](../../libraries/) 安装到 Arduino 库目录。按照[首页开发环境](../../../../README_ZH.md)设置开发板，打开本目录的 `.ino` 文件上传，再打开 115200 波特率串口监视器。

## 预期现象

串口输出温度和相对湿度。

## 常见问题

无需外接传感器或额外的传感器库。

