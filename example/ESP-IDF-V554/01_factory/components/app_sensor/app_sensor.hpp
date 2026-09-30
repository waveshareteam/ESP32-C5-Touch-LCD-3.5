/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief App_sensor application for displaying the app name on the device screen
 * 
 */
class App_sensor: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of App_sensor
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return App_sensor* Singleton instance pointer
     */
    static App_sensor *requestInstance(bool use_status_bar = true, bool use_navigation_bar = false);

    /**
     * @brief Destroy the App_sensor object
     * 
     */
    ~App_sensor();

protected:
    /**
     * @brief Construct a new App_sensor object (private to enforce singleton)
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    App_sensor(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static App_sensor *_instance;
};

} // namespace esp_brookesia::apps