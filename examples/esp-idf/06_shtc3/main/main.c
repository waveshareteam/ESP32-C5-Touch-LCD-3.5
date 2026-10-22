#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "shtc3_test";

void app_main(void)
{
    float temperature = 0.0f;
    float humidity = 0.0f;

    vTaskDelay(pdMS_TO_TICKS(1000));

    i2c_master_dev_handle_t shtc3_handle = bsp_shtc3_drv_init();
    if (shtc3_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize SHTC3");
        return;
    }

    ESP_LOGI(TAG, "SHTC3 initialized");

    while (1) {
        esp_err_t err = shtc3_get_th(shtc3_handle,
                                     SHTC3_REG_T_CSD_NM,
                                     &temperature,
                                     &humidity);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f C, Humidity: %.2f %%",
                     temperature, humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read SHTC3: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
