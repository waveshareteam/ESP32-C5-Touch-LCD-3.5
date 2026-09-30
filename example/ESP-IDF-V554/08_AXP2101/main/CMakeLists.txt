idf_component_register(SRCS "main.cpp"
                    "port_axp2101.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES XPowersLib esp_driver_i2c esp_driver_gpio)
