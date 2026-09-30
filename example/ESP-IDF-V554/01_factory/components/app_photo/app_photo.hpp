/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "lvgl.h"

namespace esp_brookesia::apps
{

    /**
     * @brief App_photo application for displaying the app name on the device screen
     *
     */
    class App_photo : public systems::phone::App
    {
    public:
        /**
         * @brief Get the singleton instance of App_photo
         *
         * @param use_status_bar Show status bar
         * @param use_navigation_bar Show navigation bar
         * @return App_photo* Singleton instance pointer
         */
        static App_photo *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

        /**
         * @brief Destroy the App_photo object
         *
         */
        ~App_photo();

    protected:
        /**
         * @brief Construct a new App_photo object (private to enforce singleton)
         *
         * @param use_status_bar Show status bar
         * @param use_navigation_bar Show navigation bar
         */
        App_photo(bool use_status_bar, bool use_navigation_bar);

        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static App_photo *_instance;

    };

} // namespace esp_brookesia::apps