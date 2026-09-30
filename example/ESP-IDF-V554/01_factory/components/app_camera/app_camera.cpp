/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_camera.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_system.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Camera"
#include "esp_lib_utils.h"

#include "camera_ui/camera_ui.h"

LV_IMG_DECLARE(img_app_camera);

#define APP_NAME "Camera"

namespace esp_brookesia::apps
{

    App_camera *App_camera::_instance = nullptr;

    App_camera *App_camera::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new App_camera(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    App_camera::App_camera(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_camera, true, use_status_bar, use_navigation_bar)
    {
    }

    App_camera::~App_camera()
    {
    }

    bool App_camera::run(void)
    {
        ESP_UTILS_LOGD("Run");
        app_camera_ui_start();
        cam_stream_start();
        return true;
    }

    bool App_camera::back(void)
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool App_camera::close(void)
    {
        ESP_UTILS_LOGD("Close");
        cam_stream_stop();
        app_camera_ui_destroy();
        return true;
    }

    bool App_camera::init()
    {
        ESP_UTILS_LOGD("Init");
        app_camera_drv_init(bsp_display_get_disp_dev());
        return true;
    }

    bool App_camera::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool App_camera::pause()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Pause");
        cam_stream_stop();
        app_camera_ui_destroy();
        return true;
    }

    bool App_camera::resume()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Resume");
        app_camera_ui_start();
        cam_stream_start();
        return true;
    }
} // namespace esp_brookesia::apps
