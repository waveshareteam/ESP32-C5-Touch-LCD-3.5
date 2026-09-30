# BSP: Waveshare ESP32-C5-Touch-LCD-3.5

Adapted from the BSP in `01_factory` for the standalone ESP-IDF 5.5.4 examples.

- LCD: ST7796, 320 x 480, SPI RGB565.
- Touch: local FT6336 driver, I2C address 0x38.
- I2C: port 0, SCL GPIO26, SDA GPIO27.
- LCD SPI: SCLK GPIO6, MOSI GPIO7, DC GPIO5, CS GPIO8.
- SD SPI: SCLK GPIO6, MOSI GPIO7, MISO GPIO2, CS GPIO9.
- IO expander: address 0x24; EXIO0 touch reset, EXIO1 LCD reset,
  EXIO5 amplifier control, EXIO6 input; PWM controls the backlight.

Call `bsp_io_expander_init()` before `bsp_display_start()`.
For normal LVGL touch input, disable FT6336 gesture mode as shown in
`02_lvgl_demo/main/main.c`.
