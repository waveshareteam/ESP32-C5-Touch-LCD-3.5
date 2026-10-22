# 03_SD_Card

[English](README.md) | [简体中文](README_ZH.md)

## 功能

挂载 microSD 卡并打印卡信息和容量。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。另需一张 microSD 卡。

## 怎么运行

使用 Arduino-ESP32 3.3.10，选择 **ESP32C5 Dev Module**，将仓库随附的 [libraries](../../libraries/) 安装到 Arduino 库目录。按照[首页开发环境](../../../../README_ZH.md)设置开发板，打开本目录的 `.ino` 文件上传，再打开 115200 波特率串口监视器。

## 预期现象

串口显示检测到的卡及容量。

## 常见问题

挂载失败时示例可能格式化卡，请先备份卡内数据。

