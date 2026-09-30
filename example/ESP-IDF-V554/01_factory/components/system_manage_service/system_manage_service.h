#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "axp2101_drv/axp2101_drv.hpp"
#include "wifi_manager/wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*sys_manage_set_percent_cb_t)(uint16_t value);
typedef uint16_t (*sys_manage_get_percent_cb_t)(void);

typedef struct
{
    const char *device_name;
    const char *firmware_name;

    uint16_t backlight_value;
    uint16_t vol_value;
    bool wifi_connected;
    wifi_mgr_mode_t wifi_mode;

    sys_manage_set_percent_cb_t set_backlight;
    sys_manage_get_percent_cb_t get_backlight;
    sys_manage_set_percent_cb_t set_volume;
    sys_manage_get_percent_cb_t get_volume;
    esp_err_t (*scan_wifi)(wifi_ap_record_t *records, uint16_t *record_count);
    bool (*get_power_info)(axp2101_power_info_t *info);
    
    qmi8658_dev_t *qmi8658_dev; 
    pcf85063a_dev_t *pcf85063a_dev;
    i2c_master_dev_handle_t shtc3_dev;
    void *axp2101_dev;
}sys_manage_service_t;

sys_manage_service_t  *system_manage_service_init(void);
sys_manage_service_t  *get_system_manage_service_handle(void);
esp_err_t system_manage_service_register_controls(sys_manage_set_percent_cb_t set_backlight,
                                                  sys_manage_get_percent_cb_t get_backlight,
                                                  sys_manage_set_percent_cb_t set_volume,
                                                  sys_manage_get_percent_cb_t get_volume);
esp_err_t system_manage_service_set_backlight(uint16_t value);
esp_err_t system_manage_service_set_volume(uint16_t value);
esp_err_t system_manage_service_scan_wifi(wifi_ap_record_t *records, uint16_t *record_count);
bool system_manage_service_get_power_info(axp2101_power_info_t *info);

#ifdef __cplusplus
}
#endif
