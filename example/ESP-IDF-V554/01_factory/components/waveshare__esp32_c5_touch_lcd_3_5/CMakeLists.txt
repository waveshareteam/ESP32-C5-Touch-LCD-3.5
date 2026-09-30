idf_component_register(
    SRCS "esp32_c5_touch_lcd_3_5.c" ${SRC_VER}
    INCLUDE_DIRS "include" "include/bsp"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES fatfs esp_driver_i2c esp_driver_gpio esp_lcd  custom_io_expander_ch32v003 waveshare__esp_lcd_touch_ft6336 shtc3 waveshare__pcf85063a waveshare__qmi8658
    PRIV_REQUIRES esp_timer spiffs esp_psram usb
)