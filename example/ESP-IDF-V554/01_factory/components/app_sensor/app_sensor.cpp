/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_sensor.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_system.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Camera"
#include "esp_lib_utils.h"

#include "ui/compass_ui.h"

LV_IMG_DECLARE(img_app_compass);

#define APP_NAME "Sensor"

namespace esp_brookesia::apps
{

    App_sensor *App_sensor::_instance = nullptr;

    App_sensor *App_sensor::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new App_sensor(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    App_sensor::App_sensor(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_compass, true, use_status_bar, use_navigation_bar)
    {
    }

    App_sensor::~App_sensor()
    {
    }

    bool App_sensor::run(void)
    {
        ESP_UTILS_LOGD("Run");
        sensor_layout_init();
        return true;
    }

    bool App_sensor::back(void)
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool App_sensor::close(void)
    {
        ESP_UTILS_LOGD("Close");
        sensor_layout_deinit();
        return true;
    }

    bool App_sensor::init()
    {
        ESP_UTILS_LOGD("Init");

        return true;
    }

    bool App_sensor::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool App_sensor::pause()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Pause");

        return true;
    }

    bool App_sensor::resume()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Resume");

        return true;
    }
} // namespace esp_brookesia::apps