# 你的原有内容
file(GLOB_RECURSE MAIN_SRCS ./*.cpp ./*.c)

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    REQUIRES       waveshare__esp32_c5_touch_lcd_3_5 nvs_flash esp_netif sdmmc
)
