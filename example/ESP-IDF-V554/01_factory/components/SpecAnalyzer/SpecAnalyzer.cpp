#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_task.h"
#include "esp_heap_caps.h" // 添加PSRAM支持头文件

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:SpecAnalyzer"
#include "esp_lib_utils.h"
#include "SpecAnalyzer.hpp"
#include <cmath>
#include "record_demo.h"
#include "esp_codec_dev.h"
#include "bsp/esp-bsp.h"

#define APP_NAME "SpecAnalyzer"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(img_app_specanalyzer);

#define APP_NAME "mic analyzer"
static esp_codec_dev_handle_t g_mic_codec_dev = NULL;

namespace esp_brookesia::apps
{

    SpecAnalyzer *SpecAnalyzer::_instance = nullptr;

    SpecAnalyzer *SpecAnalyzer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new SpecAnalyzer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    SpecAnalyzer::SpecAnalyzer(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_specanalyzer, true, use_status_bar, use_navigation_bar)
    {
    }

    SpecAnalyzer::~SpecAnalyzer()
    {
    }

    bool SpecAnalyzer::run(void)
    {
        ESP_UTILS_LOGD("Run");
        audio_record_init(g_mic_codec_dev);
        return true;
    }

    bool SpecAnalyzer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        notifyCoreClosed();
        return true;
    }

    bool SpecAnalyzer::close(void)
    {
        ESP_UTILS_LOGD("Close");
        audio_record_deinit();
        return true;
    }

    bool SpecAnalyzer::init() 
    {
        g_mic_codec_dev =  bsp_audio_codec_microphone_init();
        return true; 
    }
    bool SpecAnalyzer::deinit() 
    { 
        audio_record_deinit();
        return true; 
    }
    bool SpecAnalyzer::pause()
    {
        audio_record_stop();
        return true;
    }
    bool SpecAnalyzer::resume() { return true; }

} // namespace esp_brookesia::apps
