# ESP32-C5-Touch-LCD-3.5 BSP

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供本示例使用的显示、触摸、SD 卡和板载外设接口。

## 硬件需求

ESP32-C5-Touch-LCD-3.5；LCD 为 ST7796，触摸为 FT6336。BOOT 使用 GPIO28，低电平表示按下；PWR 通过 EXIO6 读取，高电平表示按下，EXIO6 不是主控 GPIO。

## 怎么运行

请按所属[示例说明](../../README_ZH.md)构建并烧录。显示应用先初始化 IO 扩展，再启动显示。

## 预期现象

示例使用板载接口完成对应的外设测试，测试现象见示例说明。

## 常见问题

LCD 与 SD 卡共享 SPI 信号，使用独立 CS；普通触摸输入需要关闭 FT6336 手势模式。

