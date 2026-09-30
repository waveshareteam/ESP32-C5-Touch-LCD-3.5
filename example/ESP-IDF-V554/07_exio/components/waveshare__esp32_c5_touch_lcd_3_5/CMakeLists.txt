idf_component_register(
    SRCS "esp32_c5_touch_lcd_3_5.c"
    INCLUDE_DIRS "include" "include/bsp"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES fatfs esp_driver_i2c esp_driver_gpio esp_driver_i2s esp_driver_sdspi
             esp_lcd esp_lcd_touch_ft6336 shtc3 espressif__button
             waveshare__custom_io_expander_ch32v003 waveshare__pcf85063a waveshare__qmi8658
    PRIV_REQUIRES esp_timer spiffs esp_psram esp_driver_spi espressif__esp_lcd_st7796
)
