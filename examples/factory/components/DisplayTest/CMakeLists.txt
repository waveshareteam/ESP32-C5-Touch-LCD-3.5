idf_component_register(SRCS "DisplayTest.cpp" "assets/img_app_display_test.c"
                       INCLUDE_DIRS "." REQUIRES brookesia_core lvgl app_support
                       PRIV_REQUIRES esp32_c5_touch_lcd_3_5)
