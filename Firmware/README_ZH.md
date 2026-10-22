# 固件

[English](README.md) | [简体中文](README_ZH.md)

| 文件 | 用途 |
| --- | --- |
| [ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin](ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin) | 12 个应用的出厂桌面，包含小智、SHTC3 温湿度、按键测试与显示测试；ESP-IDF v5.5.4 |

本目录仅保留最新出厂固件。合并镜像包含引导程序、分区表、应用和语音模型，按 **32 MB Flash、8 MB PSRAM** 的产品配置构建，从 **0x0** 烧录；摄像头功能需要连接 BF3901 模组。

```bash
python -m pip install esptool
python -m esptool --chip esp32c5 --port PORT write_flash 0x0 ESP32-C5-Touch-LCD-3.5_FactoryOnly_20261022.bin
```

在本目录运行，将 PORT 替换为实际串口，如 COMx 或 /dev/ttyACMx。烧录会替换固件，请先保留需要的设备设置。

SHA256: `5de2731c4a9d5100a538c1927afe3ab11b2252f08a1a073e46bf9e8a3e9fb050`.

[出厂源码](../examples/factory/README_ZH.md) | [应用使用说明](../examples/factory/USER_GUIDE_ZH.md)

