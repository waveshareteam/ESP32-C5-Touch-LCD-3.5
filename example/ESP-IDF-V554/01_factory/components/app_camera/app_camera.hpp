/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief app_camera application for displaying the app name on the device screen
 * 
 */
class App_camera: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of App_camera
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return App_camera* Singleton instance pointer
     */
    static App_camera *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the App_camera object
     * 
     */
    ~App_camera();

protected:
    /**
     * @brief Construct a new App_camera object (private to enforce singleton)
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    App_camera(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static App_camera *_instance;
};

} // namespace esp_brookesia::apps