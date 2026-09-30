/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_photo.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Photos"
#include "esp_lib_utils.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "photo_init/photo_init.h"
 
LV_IMG_DECLARE(img_app_gallery);

generic_file_list_t jpg_files;

#define APP_NAME "Photos"

namespace esp_brookesia::apps
{

    App_photo *App_photo::_instance = nullptr;

    App_photo *App_photo::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new App_photo(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    App_photo::App_photo(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_gallery, true, use_status_bar, use_navigation_bar)                                   
    {
    }

    App_photo::~App_photo()
    {

    }

    bool App_photo::run(void)
    {
        ESP_UTILS_LOGD("Run");
        init_photo_player(bsp_display_get_disp_dev());
        
        esp_err_t err = get_file_list_by_ext("/sdcard/photo",".jpg",&jpg_files);
        if (err == ESP_OK) {
            for (int i = 0; i < jpg_files.count; i++) {
                printf("JPG[%d]: %s\n", i, jpg_files.list[i]);
            }
        }
        init_photo_ui_screen(lv_screen_active(),&jpg_files);


        return true;
    }

    bool App_photo::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool App_photo::close(void)
    {
        ESP_UTILS_LOGD("Close");
        deinit_photo_player();
        return true;
    }

    bool App_photo::init()
    {
        ESP_UTILS_LOGD("Init");
        
        return true;
    }

    bool App_photo::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool App_photo::pause()
    {
        ESP_UTILS_LOGD("Pause");

        return true;
    }

    bool App_photo::resume()
    {
        ESP_UTILS_LOGD("Resume");

        return true;
    }



} // namespace esp_brookesia::apps