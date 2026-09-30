/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief App_music application for displaying the app name on the device screen
 * 
 */
class App_music: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of App_music
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return App_music* Singleton instance pointer
     */
    static App_music *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the App_music object
     * 
     */
    ~App_music();

protected:
    /**
     * @brief Construct a new App_music object (private to enforce singleton)
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    App_music(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static App_music *_instance;
};

} // namespace esp_brookesia::apps