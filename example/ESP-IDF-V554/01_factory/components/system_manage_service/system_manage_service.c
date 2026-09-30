#include "system_manage_service.h"
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "bsp_board_extra.h"

static const char *TAG = "system_manage_service";

static sys_manage_service_t *sys_manage_service = NULL;

static uint16_t clamp_percent(uint16_t value)
{
    return value > 100 ? 100 : value;
}

static esp_err_t default_set_backlight(uint16_t value)
{
    return bsp_display_brightness_set(clamp_percent(value));
}

static uint16_t default_get_backlight(void)
{
    if (sys_manage_service == NULL) {
        return 100;
    }
    return sys_manage_service->backlight_value;
}

static esp_err_t default_set_volume(uint16_t value)
{
    Volume_Adjustment((uint8_t)clamp_percent(value));
    return ESP_OK;
}

static uint16_t default_get_volume(void)
{
    return get_audio_volume();
}

static void http_server_wifi_cb(wifi_mgr_event_t event, void *event_data)
{
    switch (event) {
        case WIFI_MGR_EVENT_CONNECTED:
            if (sys_manage_service) {
                sys_manage_service->wifi_connected = true;
                sys_manage_service->wifi_mode = WIFI_MGR_MODE_STA;
            }
            ESP_LOGI(TAG, "[HTTP] Wi-Fi connected, starting HTTP server...");
            // 在这里启动 HTTP server
            break;
        case WIFI_MGR_EVENT_DISCONNECTED:
            if (sys_manage_service) {
                sys_manage_service->wifi_connected = false;
                sys_manage_service->wifi_mode = wifi_manager_get_mode();
            }
            ESP_LOGW(TAG, "[HTTP] Wi-Fi disconnected, pausing HTTP server...");
            break;
        case WIFI_MGR_EVENT_FAIL:
            if (sys_manage_service) {
                sys_manage_service->wifi_connected = false;
                sys_manage_service->wifi_mode = wifi_manager_get_mode();
            }
            ESP_LOGE(TAG, "[HTTP] Wi-Fi connection failed");
            break;
        default:
            break;
    }
}

sys_manage_service_t *system_manage_service_init(void)
{
    if (sys_manage_service != NULL)
    {
        ESP_LOGD(TAG, "system_manage_service already init");
        return sys_manage_service;
    }
    sys_manage_service = (sys_manage_service_t *)calloc(1, sizeof(sys_manage_service_t));
    ESP_RETURN_ON_FALSE(sys_manage_service != NULL, NULL, TAG, "no memory for system manage service");

    sys_manage_service->device_name = "ESP32-C5 Touch LCD 3.5";
    sys_manage_service->firmware_name = "Brookesia Phone Demo";
    sys_manage_service->backlight_value = 100;
    sys_manage_service->vol_value = VOLUME_DEFAULT;
    sys_manage_service->wifi_mode = WIFI_MGR_MODE_NONE;
    sys_manage_service->set_backlight = default_set_backlight;
    sys_manage_service->get_backlight = default_get_backlight;
    sys_manage_service->set_volume = default_set_volume;
    sys_manage_service->get_volume = default_get_volume;
    sys_manage_service->scan_wifi = system_manage_service_scan_wifi;
    sys_manage_service->get_power_info = system_manage_service_get_power_info;

    sys_manage_service->axp2101_dev = bsp_axp2101_init();  // ← 直接赋值，两边都是 void *，无需转换

    
    vTaskDelay(pdMS_TO_TICKS(500));
    sys_manage_service->pcf85063a_dev = bsp_pcf85063a_drv_init();
    sys_manage_service->qmi8658_dev = bsp_qmi8658_drv_init();
    sys_manage_service->shtc3_dev = bsp_shtc3_drv_init();

    pcf85063a_datetime_t Now_time;
    pcf85063a_get_time_date(sys_manage_service->pcf85063a_dev, &Now_time);
    sync_system_time_from_rtc(&Now_time);

    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_manager_register_callback(http_server_wifi_cb));
    ESP_ERROR_CHECK(wifi_manager_start_sta("weather-wifi", "12345678"));

    return sys_manage_service;
}

sys_manage_service_t  *get_system_manage_service_handle(void)
{
    return sys_manage_service;
}

esp_err_t system_manage_service_register_controls(sys_manage_set_percent_cb_t set_backlight,
                                                  sys_manage_get_percent_cb_t get_backlight,
                                                  sys_manage_set_percent_cb_t set_volume,
                                                  sys_manage_get_percent_cb_t get_volume)
{
    ESP_RETURN_ON_FALSE(sys_manage_service != NULL, ESP_ERR_INVALID_STATE, TAG, "service not initialized");

    if (set_backlight) {
        sys_manage_service->set_backlight = set_backlight;
    }
    if (get_backlight) {
        sys_manage_service->get_backlight = get_backlight;
        sys_manage_service->backlight_value = clamp_percent(get_backlight());
    }
    if (set_volume) {
        sys_manage_service->set_volume = set_volume;
    }
    if (get_volume) {
        sys_manage_service->get_volume = get_volume;
        sys_manage_service->vol_value = clamp_percent(get_volume());
    }

    return ESP_OK;
}

esp_err_t system_manage_service_set_backlight(uint16_t value)
{
    ESP_RETURN_ON_FALSE(sys_manage_service != NULL, ESP_ERR_INVALID_STATE, TAG, "service not initialized");
    value = clamp_percent(value);
    ESP_RETURN_ON_FALSE(sys_manage_service->set_backlight != NULL, ESP_ERR_INVALID_STATE, TAG, "backlight setter missing");

    esp_err_t ret = sys_manage_service->set_backlight(value);
    if (ret == ESP_OK) {
        sys_manage_service->backlight_value = value;
    }
    return ret;
}

esp_err_t system_manage_service_set_volume(uint16_t value)
{
    ESP_RETURN_ON_FALSE(sys_manage_service != NULL, ESP_ERR_INVALID_STATE, TAG, "service not initialized");
    value = clamp_percent(value);
    ESP_RETURN_ON_FALSE(sys_manage_service->set_volume != NULL, ESP_ERR_INVALID_STATE, TAG, "volume setter missing");

    esp_err_t ret = sys_manage_service->set_volume(value);
    if (ret == ESP_OK) {
        sys_manage_service->vol_value = value;
    }
    return ret;
}

esp_err_t system_manage_service_scan_wifi(wifi_ap_record_t *records, uint16_t *record_count)
{
    ESP_RETURN_ON_FALSE(records != NULL, ESP_ERR_INVALID_ARG, TAG, "records is NULL");
    ESP_RETURN_ON_FALSE(record_count != NULL, ESP_ERR_INVALID_ARG, TAG, "record_count is NULL");
    ESP_RETURN_ON_FALSE(*record_count > 0, ESP_ERR_INVALID_ARG, TAG, "record_count is zero");

    esp_err_t ret = wifi_manager_prepare_scan();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan preparation failed: %s", esp_err_to_name(ret));
        *record_count = 0;
        return ret;
    }

    ret = esp_wifi_scan_start(NULL, true);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        *record_count = 0;
        return ret;
    }

    return esp_wifi_scan_get_ap_records(record_count, records);
}

bool system_manage_service_get_power_info(axp2101_power_info_t *info)
{
    if (sys_manage_service == NULL || info == NULL || sys_manage_service->axp2101_dev == NULL) {
        return false;
    }
    return bsp_axp2101_get_power_info(sys_manage_service->axp2101_dev, info);
}
