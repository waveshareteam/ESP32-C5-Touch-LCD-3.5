set(srcs "main.cpp" )


               
idf_component_register(SRCS "${srcs}"
                       PRIV_REQUIRES waveshare__esp32_c5_touch_lcd_3_5 
                       esp_io_expander
                       esp_timer 
                       nvs_flash  
                       system_manage_service 
                       app_photo 
                       app_sensor
                       app_camera 
                       app_music
                       app_settings
                       DrawingBoard
                       SpecAnalyzer
                       )

# Generate storage.bin and include it in the default flash target.
spiffs_create_partition_image(storage MP3 FLASH_IN_PROJECT)
