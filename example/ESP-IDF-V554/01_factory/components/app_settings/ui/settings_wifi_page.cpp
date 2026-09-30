#include "settings_wifi_page.hpp"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "gui/lvgl/esp_brookesia_lv_lock.hpp"
#include "system_manage_service.h"
#include "settings_page_common.hpp"

#define SETTINGS_WIFI_MAX_RECORDS 10
#define SETTINGS_WIFI_SCAN_TASK_STACK_SIZE 4096
#define SETTINGS_WIFI_SCAN_TASK_PRIORITY 4

namespace {

static lv_obj_t *s_wifi_status = nullptr;
static lv_obj_t *s_wifi_list = nullptr;
static lv_obj_t *s_scan_button = nullptr;
static bool s_scan_in_progress = false;

struct WifiScanResult {
    esp_err_t ret;
    uint16_t count;
    wifi_ap_record_t records[SETTINGS_WIFI_MAX_RECORDS];
};

static void set_wifi_status(const char *text)
{
    if (s_wifi_status != nullptr) {
        lv_label_set_text(s_wifi_status, text);
    }
}

static void add_wifi_network(const wifi_ap_record_t &record)
{
    char ssid[33] = {};
    memcpy(ssid, record.ssid, sizeof(record.ssid));
    if (ssid[0] == '\0') {
        strcpy(ssid, "<hidden>");
    }

    lv_obj_t *row = lv_list_add_button(s_wifi_list, LV_SYMBOL_WIFI, ssid);
    settings_ui::style_list_row(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *rssi = lv_label_create(row);
    lv_label_set_text_fmt(rssi, "%d dBm", record.rssi);
    settings_ui::style_text(rssi, 0x697386);
    lv_obj_align(rssi, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void set_scan_button_enabled(bool enabled)
{
    if (s_scan_button == nullptr) {
        return;
    }

    if (enabled) {
        lv_obj_remove_state(s_scan_button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_scan_button, LV_STATE_DISABLED);
    }
}

static void add_wifi_list_header(void)
{
    if (s_wifi_list == nullptr) {
        return;
    }

    lv_obj_t *hint = lv_list_add_text(s_wifi_list, "Nearby networks");
    settings_ui::style_list_header(hint);
}

static void finish_wifi_scan(esp_err_t ret, const wifi_ap_record_t *records, uint16_t count)
{
    s_scan_in_progress = false;
    set_scan_button_enabled(true);
    if (s_wifi_status == nullptr || s_wifi_list == nullptr) {
        return;
    }

    if (ret != ESP_OK) {
        lv_label_set_text_fmt(s_wifi_status, "Scan failed: %s", esp_err_to_name(ret));
        return;
    }

    lv_obj_clean(s_wifi_list);
    add_wifi_list_header();
    lv_label_set_text_fmt(s_wifi_status, "%u network%s found", count, count == 1 ? "" : "s");
    for (uint16_t i = 0; i < count; i++) {
        add_wifi_network(records[i]);
    }
}

static void finish_wifi_scan_async(void *arg)
{
    WifiScanResult *result = static_cast<WifiScanResult *>(arg);
    finish_wifi_scan(result->ret, result->records, result->count);
    free(result);
}

static void wifi_scan_task(void *arg)
{
    sys_manage_service_t *sms = static_cast<sys_manage_service_t *>(arg);
    WifiScanResult *result = static_cast<WifiScanResult *>(calloc(1, sizeof(WifiScanResult)));
    if (result != nullptr) {
        result->count = SETTINGS_WIFI_MAX_RECORDS;
        result->ret = sms->scan_wifi(result->records, &result->count);
        if (result->ret != ESP_OK) {
            result->count = 0;
        }

        /* Only queue work while holding the LVGL lock.  The callback itself is
         * run later by LVGL after this task has exited and released its stack. */
        esp_brookesia::gui::LvLockGuard gui_guard;
        if (lv_async_call(finish_wifi_scan_async, result) != LV_RESULT_OK) {
            free(result);
        }
    }
    vTaskDelete(nullptr);
}

static void scan_wifi_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_scan_in_progress) {
        return;
    }

    sys_manage_service_t *sms = get_system_manage_service_handle();
    if (sms == nullptr || sms->scan_wifi == nullptr) {
        set_wifi_status("System service is not ready");
        return;
    }

    s_scan_button = static_cast<lv_obj_t *>(lv_event_get_target(e));
    s_scan_in_progress = true;
    set_scan_button_enabled(false);
    set_wifi_status("Scanning nearby networks...");
    if (xTaskCreate(wifi_scan_task, "settings_wifi_scan", SETTINGS_WIFI_SCAN_TASK_STACK_SIZE,
                    sms, SETTINGS_WIFI_SCAN_TASK_PRIORITY, nullptr) != pdPASS) {
        s_scan_in_progress = false;
        set_scan_button_enabled(true);
        set_wifi_status("Scan task could not start");
    }
}

static void wifi_page_delete_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    s_wifi_status = nullptr;
    s_wifi_list = nullptr;
    s_scan_button = nullptr;
}

} // namespace

lv_obj_t *settings_wifi_page_create(lv_obj_t *parent, lv_event_cb_t back_cb)
{
    lv_obj_t *page = lv_obj_create(parent);
    settings_ui::style_sheet(page);
    settings_ui::create_header(page, "WLAN", back_cb, scan_wifi_event_cb);
    lv_obj_add_event_cb(page, wifi_page_delete_event_cb, LV_EVENT_DELETE, nullptr);

    s_wifi_status = lv_label_create(page);
    settings_ui::style_text(s_wifi_status, 0x697386);
    lv_obj_set_width(s_wifi_status, 292);
    lv_label_set_long_mode(s_wifi_status, LV_LABEL_LONG_DOT);
    lv_obj_align(s_wifi_status, LV_ALIGN_TOP_LEFT, 6, 48);

    sys_manage_service_t *sms = get_system_manage_service_handle();
    lv_label_set_text_fmt(s_wifi_status, "Wi-Fi %s. Tap refresh to scan.",
                          sms && sms->wifi_connected ? "connected" : "not connected");

    s_wifi_list = lv_list_create(page);
    lv_obj_set_size(s_wifi_list, 304, 348);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_MID, 0, 76);
    settings_ui::style_list(s_wifi_list);

    add_wifi_list_header();

    return page;
}
