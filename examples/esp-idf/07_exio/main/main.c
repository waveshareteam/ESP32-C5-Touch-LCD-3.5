#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io_extension.h"
void app_main(void)
{
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(IO_EXTENSION_Init(BSP_I2C_NUM));

    const uint8_t first_pin = IO_EXTENSION_IO_8;
    const uint8_t last_pin = IO_EXTENSION_IO_15;
    bool level = false;

    ESP_LOGI("exio", "Toggle EXIO8-15 every 1000 ms; EXIO0-7 excluded");

    while (1) {
        for (uint8_t pin = first_pin; pin <= last_pin; pin++) {
            ESP_ERROR_CHECK(IO_EXTENSION_Output(pin, level));
        }

        level = !level;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
