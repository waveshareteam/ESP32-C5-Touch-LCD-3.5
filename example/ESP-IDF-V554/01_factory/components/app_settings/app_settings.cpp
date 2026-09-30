/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_settings.hpp"

#include "esp_brookesia.hpp"
#include "esp_err.h"
#include "esp_io_expander.h"
#include "esp_log.h"
#include "system_manage_service.h"
#include "ui/settings_about_page.hpp"
#include "ui/settings_page_common.hpp"
#include "ui/settings_power_page.hpp"
#include "ui/settings_wifi_page.hpp"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Settings"
#include "esp_lib_utils.h"

#define APP_NAME "Settings"

LV_IMG_DECLARE(img_app_settings);
LV_IMG_DECLARE(WIFI);
LV_IMG_DECLARE(backlights);
LV_IMG_DECLARE(sound);
LV_IMG_DECLARE(battery);
LV_IMG_DECLARE(info);
LV_IMG_DECLARE(arrow);

namespace {

static const char *TAG = "settings_ui";
static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_main_page = nullptr;
static lv_obj_t *s_wifi_page = nullptr;
static lv_obj_t *s_power_page = nullptr;
static lv_obj_t *s_about_page = nullptr;
static lv_obj_t *s_exio6_status_label = nullptr;
static lv_timer_t *s_exio6_timer = nullptr;
static bool s_exio6_last_pressed = false;
static lv_obj_t *s_rtc_status_label = nullptr;
static lv_timer_t *s_rtc_timer = nullptr;

constexpr uint32_t EXIO6_PIN_MASK = IO_EXPANDER_PIN_NUM_6;
constexpr uint32_t EXIO6_SCAN_PERIOD_MS = 100;
constexpr uint32_t RTC_REFRESH_PERIOD_MS = 1000;

static void show_page(lv_obj_t *page)
{
    if (page != s_power_page) {
        settings_power_page_stop();
    }

    lv_obj_t *pages[] = {s_main_page, s_wifi_page, s_power_page, s_about_page};
    for (lv_obj_t *item : pages) {
        if (item != nullptr) {
            lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (page != nullptr) {
        lv_obj_remove_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_main_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        show_page(s_main_page);
    }
}

static void wifi_entry_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_wifi_page == nullptr) {
        ESP_LOGI(TAG, "create WLAN page");
        s_wifi_page = settings_wifi_page_create(s_root, show_main_event_cb);
    }
    show_page(s_wifi_page);
}

static void power_entry_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_power_page == nullptr) {
        ESP_LOGI(TAG, "create power page");
        s_power_page = settings_power_page_create(s_root, show_main_event_cb);
    }
    show_page(s_power_page);
    settings_power_page_start();
}

static void about_entry_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_about_page == nullptr) {
        ESP_LOGI(TAG, "create about page");
        s_about_page = settings_about_page_create(s_root, show_main_event_cb);
    }
    show_page(s_about_page);
}

static void append_arrow(lv_obj_t *row)
{
    lv_obj_t *arrow_img = lv_image_create(row);
    lv_image_set_src(arrow_img, &arrow);
    lv_obj_set_size(arrow_img, 16, 16);
    lv_obj_set_style_image_recolor(arrow_img, lv_color_hex(0x2266DD), LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(arrow_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(arrow_img, LV_ALIGN_RIGHT_MID, -4, 0);
}

static bool read_exio6_pressed(bool *pressed)
{
    esp_io_expander_handle_t io_expander = bsp_get_io_expander();
    if (io_expander == nullptr) {
        return false;
    }

    uint32_t level_mask = 0;
    esp_err_t ret = esp_io_expander_get_level(io_expander, EXIO6_PIN_MASK, &level_mask);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read EXIO6 failed: %s", esp_err_to_name(ret));
        return false;
    }

    *pressed = (level_mask & EXIO6_PIN_MASK) != 0;
    return true;
}

static void update_exio6_status_label(bool pressed)
{
    if (s_exio6_status_label == nullptr) {
        return;
    }

    lv_label_set_text(s_exio6_status_label, pressed ? "Pressed" : "Released");
}

static void exio6_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bool pressed = false;
    if (!read_exio6_pressed(&pressed)) {
        if (s_exio6_status_label != nullptr) {
            lv_label_set_text(s_exio6_status_label, "N/A");
        }
        return;
    }

    if (pressed != s_exio6_last_pressed) {
        ESP_LOGI(TAG, "EXIO6 %s", pressed ? "pressed" : "released");
        update_exio6_status_label(pressed);
        s_exio6_last_pressed = pressed;
    }
}

static void add_arrow_row(lv_obj_t *list, const lv_image_dsc_t *icon, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *row = lv_list_add_button(list, icon, text);
    settings_ui::style_list_row(row);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);
    append_arrow(row);
}

static void add_exio6_row(lv_obj_t *list)
{
    lv_obj_t *row = lv_list_add_button(list, &info, "EXIO6");
    settings_ui::style_list_row(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    s_exio6_status_label = lv_label_create(row);
    settings_ui::style_text(s_exio6_status_label, 0x2266DD);
    lv_obj_set_width(s_exio6_status_label, 92);
    lv_label_set_long_mode(s_exio6_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_exio6_status_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text(s_exio6_status_label, "Released");
    lv_obj_align(s_exio6_status_label, LV_ALIGN_RIGHT_MID, -4, 0);

    esp_io_expander_handle_t io_expander = bsp_get_io_expander();
    if (io_expander == nullptr) {
        lv_label_set_text(s_exio6_status_label, "N/A");
        ESP_LOGW(TAG, "EXIO6 unavailable: IO expander is NULL");
        return;
    }

    bool pressed = false;
    if (read_exio6_pressed(&pressed)) {
        s_exio6_last_pressed = pressed;
        update_exio6_status_label(pressed);
    }

    if (s_exio6_timer == nullptr) {
        s_exio6_timer = lv_timer_create(exio6_timer_cb, EXIO6_SCAN_PERIOD_MS, nullptr);
    }
}

static void update_rtc_status(void)
{
    if (s_rtc_status_label == nullptr) {
        return;
    }

    sys_manage_service_t *sms = get_system_manage_service_handle();
    if (sms == nullptr || sms->pcf85063a_dev == nullptr) {
        lv_label_set_text(s_rtc_status_label, "N/A");
        return;
    }

    pcf85063a_datetime_t now = {};
    esp_err_t ret = pcf85063a_get_time_date(sms->pcf85063a_dev, &now);
    if (ret != ESP_OK) {
        lv_label_set_text(s_rtc_status_label, "Read error");
        return;
    }

    lv_label_set_text_fmt(s_rtc_status_label, "%02u:%02u:%02u",
                          (unsigned)now.hour, (unsigned)now.min, (unsigned)now.sec);
}

static void rtc_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_rtc_status();
}

static void add_rtc_row(lv_obj_t *list)
{
    lv_obj_t *row = lv_list_add_button(list, &info, "RTC");
    settings_ui::style_list_row(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    s_rtc_status_label = lv_label_create(row);
    settings_ui::style_text(s_rtc_status_label, 0x2266DD);
    lv_obj_set_width(s_rtc_status_label, 92);
    lv_obj_set_style_text_align(s_rtc_status_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text(s_rtc_status_label, "--:--:--");

    update_rtc_status();
    if (s_rtc_timer == nullptr) {
        s_rtc_timer = lv_timer_create(rtc_timer_cb, RTC_REFRESH_PERIOD_MS, nullptr);
    }
}

static void backlight_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int value = (int)lv_slider_get_value(slider);
    ESP_ERROR_CHECK_WITHOUT_ABORT(system_manage_service_set_backlight((uint16_t)value));
}

static void volume_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int value = (int)lv_slider_get_value(slider);
    ESP_ERROR_CHECK_WITHOUT_ABORT(system_manage_service_set_volume((uint16_t)value));
}

static void add_slider_row(lv_obj_t *list, const lv_image_dsc_t *icon, const char *text,
                           uint16_t value, lv_event_cb_t cb)
{
    lv_obj_t *row = lv_list_add_button(list, icon, text);
    settings_ui::style_list_row(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, 122, 22);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, value > 100 ? 100 : value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE1E7F0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2266DD), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2266DD), LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    // Apply the hardware setting once the drag ends. Writing on every pixel
    // movement performs synchronous device I/O in the LVGL task and causes lag.
    lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, nullptr);
}

static void create_main_page(void)
{
    ESP_LOGI(TAG, "create main page");
    s_main_page = lv_obj_create(s_root);
    settings_ui::style_sheet(s_main_page);
    settings_ui::create_header(s_main_page, "Settings", nullptr, nullptr);

    lv_obj_t *list = lv_list_create(s_main_page);
    lv_obj_set_size(list, 304, 374);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    settings_ui::style_list(list);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_list_add_text(list, "Wireless");
    settings_ui::style_list_header(header);
    add_arrow_row(list, &WIFI, "WLAN", wifi_entry_event_cb);

    sys_manage_service_t *sms = get_system_manage_service_handle();
    uint16_t volume = sms && sms->get_volume ? sms->get_volume() : 0;
    uint16_t backlight = sms && sms->get_backlight ? sms->get_backlight() : 100;

    header = lv_list_add_text(list, "Media");
    settings_ui::style_list_header(header);
    add_slider_row(list, &sound, "Sound", volume, volume_event_cb);
    add_slider_row(list, &backlights, "Display", backlight, backlight_event_cb);

    header = lv_list_add_text(list, "System");
    settings_ui::style_list_header(header);
    add_arrow_row(list, &battery, "Power Management", power_entry_event_cb);
    add_arrow_row(list, &info, "About", about_entry_event_cb);
    add_rtc_row(list);
    add_exio6_row(list);
    ESP_LOGI(TAG, "main page ready");
}

static void create_settings_ui(void)
{
    if (s_root != nullptr) {
        lv_obj_delete(s_root);
    }

    ESP_LOGI(TAG, "create root");
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 320, 480);
    lv_obj_align(s_root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    create_main_page();
    show_page(s_main_page);
    ESP_LOGI(TAG, "root ready");
}

static void destroy_settings_ui(void)
{
    settings_power_page_stop();

    if (s_rtc_timer != nullptr) {
        lv_timer_delete(s_rtc_timer);
        s_rtc_timer = nullptr;
    }

    if (s_exio6_timer != nullptr) {
        lv_timer_delete(s_exio6_timer);
        s_exio6_timer = nullptr;
    }

    if (s_root != nullptr) {
        lv_obj_delete(s_root);
    }
    s_root = nullptr;
    s_main_page = nullptr;
    s_wifi_page = nullptr;
    s_power_page = nullptr;
    s_about_page = nullptr;
    s_exio6_status_label = nullptr;
    s_exio6_last_pressed = false;
    s_rtc_status_label = nullptr;
}

} // namespace

namespace esp_brookesia::apps
{

App_settings *App_settings::_instance = nullptr;

App_settings *App_settings::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new App_settings(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

App_settings::App_settings(bool use_status_bar, bool use_navigation_bar) :
    App(APP_NAME, &img_app_settings, true, use_status_bar, use_navigation_bar)
{
}

App_settings::~App_settings()
{
}

bool App_settings::run(void)
{
    ESP_UTILS_LOGD("Run");
    ESP_LOGI(TAG, "run begin");
    create_settings_ui();
    ESP_LOGI(TAG, "run end");
    return true;
}

bool App_settings::back(void)
{
    ESP_UTILS_LOGD("Back");
    destroy_settings_ui();
    return notifyCoreClosed();
}

bool App_settings::close(void)
{
    ESP_UTILS_LOGD("Close");
    destroy_settings_ui();
    return true;
}

bool App_settings::init()
{
    ESP_UTILS_LOGD("Init");
    return true;
}

bool App_settings::deinit()
{
    ESP_UTILS_LOGD("Deinit");
    destroy_settings_ui();
    return true;
}

bool App_settings::pause()
{
    ESP_UTILS_LOGD("Pause");
    return true;
}

bool App_settings::resume()
{
    ESP_UTILS_LOGD("Resume");
    return true;
}

} // namespace esp_brookesia::apps
