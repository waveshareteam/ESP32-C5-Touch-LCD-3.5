idf_component_register(SRCS io_extension.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_netif esp_driver_i2c esp_driver_gpio)
