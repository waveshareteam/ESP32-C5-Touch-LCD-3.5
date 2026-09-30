#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    static const char *TAG = "pcf85063a_test";
    pcf85063a_dev_t *rtc = bsp_pcf85063a_drv_init();

    // Set the demo start time once on each boot.
    const pcf85063a_datetime_t start_time = {
        .year = 2026,
        .month = 9,
        .day = 22,
        .dotw = 2, // Tuesday; 0 = Sunday.
        .hour = 15,
        .min = 0,
        .sec = 0,
    };
    esp_err_t ret = pcf85063a_set_time_date(rtc, start_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RTC time: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        pcf85063a_datetime_t now = {0};
        esp_err_t err = pcf85063a_get_time_date(rtc, &now);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "%04u-%02u-%02u %02u:%02u:%02u, week: %u",
                     now.year, now.month, now.day,
                     now.hour, now.min, now.sec, now.dotw);
        } else {
            ESP_LOGE(TAG, "Failed to read RTC time: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
