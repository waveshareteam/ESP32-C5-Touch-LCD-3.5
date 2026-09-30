/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_music.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_system.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Music"
#include "esp_lib_utils.h"

#include "ui/lvgl_music.h"

LV_IMG_DECLARE(img_app_musicplayer);

#define APP_NAME "Music"

namespace esp_brookesia::apps
{

    App_music *App_music::_instance = nullptr;

    App_music *App_music::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new App_music(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    App_music::App_music(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_musicplayer, true, use_status_bar, use_navigation_bar)
    {
    }

    App_music::~App_music()
    {
    }

    bool App_music::run(void)
    {
        ESP_UTILS_LOGD("Run");
        music_ui_create(lv_screen_active());
        return true;
    }

    bool App_music::back(void)
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool App_music::close(void)
    {
        ESP_UTILS_LOGD("Close");
        music_ui_stop();
        music_ui_destroy();
        return true;
    }

    bool App_music::init()
    {
        ESP_UTILS_LOGD("Init");
        music_player_init();
        return true;
    }

    bool App_music::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        music_ui_stop();
        music_ui_destroy();
        return true;
    }

    bool App_music::pause()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Pause");
        music_ui_pause();
        return true;
    }

    bool App_music::resume()
    {
        ESP_LOGI(ESP_UTILS_LOG_TAG,"Resume");
        music_ui_resume();
        return true;
    }
} // namespace esp_brookesia::apps
