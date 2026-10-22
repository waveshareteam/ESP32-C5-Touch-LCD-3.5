#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "esp_lv_adapter.h"
#include <time.h>
#include "sdmmc_cmd.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lv_demos.h"
#include "esp_lcd_touch_ft6336.h"



void app_main(void)
{
    esp_err_t ret;
    esp_log_level_set("wifi", ESP_LOG_ERROR);

    // NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(bsp_io_expander_init());

    lv_display_t *disp = bsp_display_start();
    ESP_ERROR_CHECK(disp ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));

    ESP_ERROR_CHECK(esp_lcd_touch_ft6336_set_gesture_enabled(bsp_display_get_touch_handle(), false));

    lv_demo_benchmark();
    esp_lv_adapter_unlock();
}
