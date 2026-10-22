# 02_sd_card

[English](README.md) | [简体中文](README_ZH.md)

## 功能

挂载 microSD 卡，写入并回读 /test.txt 验证数据。

## 硬件需求

ESP32-C5-Touch-LCD-3.5 和 USB 数据线。另需一张 microSD 卡。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行：

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

## 预期现象

读写内容一致后，串口输出 TF CARD TEST PASS。

## 常见问题

使用 FAT32 卡；该测试会覆盖 /test.txt，请先备份已有文件。

