/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "bsp/esp-bsp.h"


namespace esp_brookesia::apps
{

    /**
     * @brief App_settings application for displaying a list of options
     *
     */
    class App_settings : public systems::phone::App
    {
    public:
        static App_settings *requestInstance(bool use_status_bar = true, bool use_navigation_bar = false);
        ~App_settings();
        bool run(void) override;

    protected:
        App_settings(bool use_status_bar, bool use_navigation_bar);

        // bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;
        
    private:
        static App_settings *_instance;

    };

} // namespace esp_brookesia::apps