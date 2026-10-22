# ESP32-C5-Touch-LCD-3.5 出厂应用

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供 320 × 480 竖屏桌面及 12 个应用，涵盖触摸、显示、拍照、音乐、录音、联网与小智对话。操作步骤见[使用说明](USER_GUIDE_ZH.md)。

## 硬件需求

ESP32-C5-Touch-LCD-3.5，32 MB Flash、8 MB PSRAM，USB 数据线。相册、音乐和录音文件使用 FAT32 microSD 卡，语音输出需连接扬声器。

## 怎么运行

在 ESP-IDF v5.5.4 环境进入本目录执行以下命令，或使用[出厂合并固件](../../firmware/README_ZH.md)。

```bash
idf.py set-target esp32c5
idf.py build flash monitor
```

合并完整固件：

```bash
idf.py merge-bin -o ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin -f raw
```

## 预期现象

启动后背光点亮并进入三页应用桌面，左右滑动切页。短按 PWR 一次开机，长按 5 秒关机；Display Test 中双击屏幕返回桌面。

## 常见问题

首次烧录请包含分区表和语音模型，单独烧录应用 BIN 不会安装模型。Weather 和 AIChats 需要可访问互联网的 Wi-Fi；小智摄像头识别还需要服务端提供视觉服务。

