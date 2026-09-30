
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"

#include "system_manage_service.h"
#include "bsp/esp-bsp.h"
#include "esp_lcd_touch_ft6336.h"

#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

#include "app_photo.hpp"
#include "app_camera.hpp"
#include "app_music.hpp"
#include "app_sensor.hpp"
#include "app_settings.hpp"
#include "DrawingBoard.hpp"
#include "SpecAnalyzer.hpp"

#include "bsp_board_extra.h"

static const char *TAG = "main";

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;
using namespace esp_brookesia::apps;
sys_manage_service_t *sms;
systems::phone::Phone *phone = nullptr;  // 声明为全局变量
constexpr bool EXAMPLE_SHOW_MEM_INFO = false;

#define COLOR_TEST_INTERVAL_MS      1000
#define GESTURE_POLL_INTERVAL_MS    20
#define DISPLAY_LOCK_TIMEOUT_MS     1000

#define WAV_TEST_FILE_PATH          "/sdcard/mic_10s_test.wav"
#define WAV_TEST_SAMPLE_RATE        16000
#define WAV_TEST_CHANNELS           1
#define WAV_TEST_BITS_PER_SAMPLE    16
#define WAV_TEST_SECONDS            10
#define WAV_TEST_READ_BYTES         1024

typedef struct __attribute__((packed)) {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} wav_test_header_t;

static wav_test_header_t create_wav_test_header(uint32_t data_size)
{
    wav_test_header_t header = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = 36 + data_size,
        .wave = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .fmt_size = 16,
        .format = 1,
        .channels = WAV_TEST_CHANNELS,
        .sample_rate = WAV_TEST_SAMPLE_RATE,
        .byte_rate = WAV_TEST_SAMPLE_RATE * WAV_TEST_CHANNELS * (WAV_TEST_BITS_PER_SAMPLE / 8),
        .block_align = WAV_TEST_CHANNELS * (WAV_TEST_BITS_PER_SAMPLE / 8),
        .bits_per_sample = WAV_TEST_BITS_PER_SAMPLE,
        .data = {'d', 'a', 't', 'a'},
        .data_size = data_size,
    };
    return header;
}

static void record_wav_10s_test(void)
{
    esp_codec_dev_handle_t mic_codec_dev = bsp_audio_codec_microphone_init();
    if (mic_codec_dev == nullptr) {
        ESP_LOGE(TAG, "WAV test microphone init failed");
        return;
    }

    FILE *wav_file = fopen(WAV_TEST_FILE_PATH, "wb+");
    if (wav_file == nullptr) {
        ESP_LOGE(TAG, "WAV test open %s failed: errno=%d", WAV_TEST_FILE_PATH, errno);
        return;
    }

    wav_test_header_t header = create_wav_test_header(0);
    if (fwrite(&header, sizeof(header), 1, wav_file) != 1) {
        ESP_LOGE(TAG, "WAV test header write failed");
        fclose(wav_file);
        return;
    }

    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = WAV_TEST_BITS_PER_SAMPLE,
        .channel = WAV_TEST_CHANNELS,
        .sample_rate = WAV_TEST_SAMPLE_RATE,
    };
    esp_codec_dev_set_in_gain(mic_codec_dev, 30.0);
    if (esp_codec_dev_open(mic_codec_dev, &sample_info) != ESP_OK) {
            ESP_LOGE(TAG, "WAV test microphone open failed");
        fclose(wav_file);
        return;
    }

    uint8_t buffer[WAV_TEST_READ_BYTES] = {};
    const uint32_t expected_data_size =
        WAV_TEST_SAMPLE_RATE * WAV_TEST_CHANNELS * (WAV_TEST_BITS_PER_SAMPLE / 8) * WAV_TEST_SECONDS;
    uint32_t recorded_data_size = 0;

    ESP_LOGI(TAG, "WAV test recording 10 seconds to %s", WAV_TEST_FILE_PATH);
    while (recorded_data_size < expected_data_size) {
        size_t read_size = expected_data_size - recorded_data_size;
        if (read_size > sizeof(buffer)) {
            read_size = sizeof(buffer);
        }

        if (esp_codec_dev_read(mic_codec_dev, buffer, read_size) != ESP_OK) {
            ESP_LOGE(TAG, "WAV test microphone read failed after %lu bytes",
                     (unsigned long)recorded_data_size);
            break;
        }

        size_t write_size = fwrite(buffer, 1, read_size, wav_file);
        recorded_data_size += write_size;
        if (write_size != read_size) {
            ESP_LOGE(TAG, "WAV test audio write failed after %lu bytes",
                     (unsigned long)recorded_data_size);
            break;
        }
    }

    esp_codec_dev_close(mic_codec_dev);

    header = create_wav_test_header(recorded_data_size);
    fseek(wav_file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, wav_file);
    fclose(wav_file);

    ESP_LOGI(TAG, "WAV test saved %lu bytes to %s",
             (unsigned long)recorded_data_size, WAV_TEST_FILE_PATH);
}


static void wifi_evt_cb(wifi_mgr_event_t event, void *event_data)
{
    switch (event) {
        case WIFI_MGR_EVENT_CONNECTED:
            bsp_display_lock(200);
            phone->getDisplay().getStatusBar()->setWifiIconState(3);
            bsp_display_unlock();
            break;
        case WIFI_MGR_EVENT_DISCONNECTED:
            bsp_display_lock(200);
            phone->getDisplay().getStatusBar()->setWifiIconState(0);
            bsp_display_unlock();
            break;
        case WIFI_MGR_EVENT_FAIL:

            break;
        default:
            break;
    }
}

static void i2c_scan_devices(void)
{
    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();
    if (i2c_bus_handle == nullptr) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return;
    }

    uint8_t found_count = 0;
    ESP_LOGI(TAG, "I2C scan start");

    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        esp_err_t ret = i2c_master_probe(i2c_bus_handle, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at address 0x%02X", addr);
            found_count++;
        }
    }

    ESP_LOGI(TAG, "I2C scan done, found %u device(s)", (unsigned)found_count);
}

static void run_color_gesture_test(esp_lcd_touch_handle_t touch_handle, lv_indev_t *input_dev)
{
    static const uint32_t test_colors[] = {
        0xFF0000,
        0x00FF00,
        0x0000FF,
        0xFFFF00,
        0xFFFFFF,
    };

    ESP_ERROR_CHECK(bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS));
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_hex(test_colors[0]), LV_PART_MAIN);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Color test started, double tap to enter the App interface");

    size_t color_index = 0;
    TickType_t last_color_tick = xTaskGetTickCount();
    while (true) {
        ESP_ERROR_CHECK(bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS));
        uint8_t gesture = esp_lcd_touch_ft6336_get_gesture(touch_handle);
        bsp_display_unlock();

        if (gesture == ESP_LCD_TOUCH_FT6336_GESTURE_DOUBLE_CLICK) {
            ESP_LOGI(TAG, "Double-click gesture detected (0x%02X)", gesture);
            break;
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_color_tick) >= pdMS_TO_TICKS(COLOR_TEST_INTERVAL_MS)) {
            color_index = (color_index + 1) % (sizeof(test_colors) / sizeof(test_colors[0]));

            ESP_ERROR_CHECK(bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS));
            lv_obj_set_style_bg_color(screen, lv_color_hex(test_colors[color_index]), LV_PART_MAIN);
            bsp_display_unlock();

            last_color_tick = now;
        }

        vTaskDelay(pdMS_TO_TICKS(GESTURE_POLL_INTERVAL_MS));
    }

    ESP_ERROR_CHECK(bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS));
    esp_err_t ret = esp_lcd_touch_ft6336_set_gesture_enabled(touch_handle, false);
    lv_indev_reset(input_dev, nullptr);
    lv_obj_clean(screen);
    bsp_display_unlock();
    ESP_ERROR_CHECK(ret);

    vTaskDelay(pdMS_TO_TICKS(100));
}

extern "C" void app_main(void)
{
    sms = system_manage_service_init();
    
    gpio_set_direction(BSP_LCD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_LCD_CS, 1);

    bsp_io_expander_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    bsp_sdcard_mount();
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_spiffs_mount());
    //record_wav_10s_test();

    lv_display_t *disp = bsp_display_start();
    lv_indev_t *tp = bsp_display_get_input_dev();
    esp_lcd_touch_handle_t touch_handle = bsp_display_get_touch_handle();
    bsp_display_backlight_on();

    ESP_ERROR_CHECK(touch_handle != nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    run_color_gesture_test(touch_handle, tp);

    /* Configure GUI lock */
    LvLock::registerCallbacks([](int timeout_ms) {
        esp_err_t ret = bsp_display_lock(timeout_ms);
        ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Lock failed (timeout_ms: %d)", timeout_ms);

        return true;
    }, []() {
        bsp_display_unlock();
        return true;
    });

    /* Create a phone object */
    ESP_LOGI(TAG, "Create phone object");
    phone = new systems::phone::Phone(disp);
    phone->setTouchDevice(tp);
    
    Stylesheet *stylesheet = new systems::phone::Stylesheet(STYLESHEET_320_480_DARK);
    ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

    ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
    ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
    ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
    delete stylesheet;

    {
        LvLockGuard gui_guard;

        /* Begin the phone */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");

        auto app1 = esp_brookesia::apps::App_settings::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app1),"start Drawpanel failed");

        auto app4 = esp_brookesia::apps::App_photo::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app4),"start Drawpanel failed");

        auto app2 = esp_brookesia::apps::App_camera::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app2),"start Drawpanel failed");

        auto app3 = esp_brookesia::apps::App_music::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app3),"start Drawpanel failed");

        auto app5 = esp_brookesia::apps::App_sensor::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app5),"start Drawpanel failed");

        auto app6 = esp_brookesia::apps::DrawingBoard::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app6), "install DrawingBoard failed");

        auto app7 = esp_brookesia::apps::SpecAnalyzer::requestInstance();
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installApp(app7),"install recorder failed");

        /* Create a timer to update the clock */
        lv_timer_create([](lv_timer_t *t) {
            time_t now;
            struct tm timeinfo;
            Phone *phone = (Phone *)t->user_data;
            static int last_clock_minute = -1;


            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

            time(&now);
            localtime_r(&now, &timeinfo);
            if (last_clock_minute == timeinfo.tm_min) {
                return;
            }
            last_clock_minute = timeinfo.tm_min;

            ESP_UTILS_CHECK_FALSE_EXIT(
                phone->getDisplay().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
                "Refresh status bar failed"
            );
            
        }, 1000, phone);
    }

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[128];    /* Make sure buffer is enough for `sprintf` */
            size_t internal_free = 0;
            size_t internal_total = 0;
            size_t external_free = 0;
            size_t external_total = 0;

            while (1) 
            {

                internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
                external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
                sprintf(buffer,
                        "\t           Biggest /     Free /    Total\n"
                        "\t  SRAM : [%8d / %8d / %8d]\n"
                        "\t PSRAM : [%8d / %8d / %8d]",
                        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);
                //ESP_UTILS_LOGI("\n%s", buffer);
                
                {
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getRecentsScreen()->setMemoryLabel(
                            internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024
                        ), "Set memory label failed"
                    );
                }

                boost::this_thread::sleep_for(boost::chrono::seconds(2));
            }
        }).detach();
    }
}
