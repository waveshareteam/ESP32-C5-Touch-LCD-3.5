# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
set(PROJECT_VER "V1.0")
project(esp32_c5_touch_lcd_3_5)

idf_component_get_property(xiaozhi_font_lib 78__xiaozhi-fonts COMPONENT_LIB)
idf_component_get_property(xiaozhi_font_dir 78__xiaozhi-fonts COMPONENT_DIR)
set_property(TARGET ${xiaozhi_font_lib} PROPERTY SOURCES
    "${xiaozhi_font_dir}/src/font_puhui_16_4.c")
