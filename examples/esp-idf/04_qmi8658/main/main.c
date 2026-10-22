#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "qmi8658_test";

void app_main(void)
{
    qmi8658_data_t data;
    qmi8658_dev_t *qmi8658 = bsp_qmi8658_drv_init();
    if (qmi8658 == NULL) {
        ESP_LOGE(TAG, "QMI8658 initialization failed; check power and I2C connection");
        return;
    }

    qmi8658_set_accel_unit_mps2(qmi8658, true);
    qmi8658_set_gyro_unit_dps(qmi8658, true);

    ESP_LOGI(TAG, "QMI8658 six-axis test started");

    while (1) {
        bool ready = false;
        esp_err_t ret = qmi8658_is_data_ready(qmi8658, &ready);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get data-ready status: %s", esp_err_to_name(ret));
        } else if (ready) {
            ret = qmi8658_read_sensor_data(qmi8658, &data);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG,
                         "Accel[m/s^2] X=%8.4f Y=%8.4f Z=%8.4f | "
                         "Gyro[dps] X=%8.4f Y=%8.4f Z=%8.4f",
                         data.accelX, data.accelY, data.accelZ,
                         data.gyroX, data.gyroY, data.gyroZ);
            } else {
                ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
