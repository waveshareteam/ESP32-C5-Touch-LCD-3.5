#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "sdmmc_cmd.h"

static const char *TAG = "tf_card_test";

static esp_err_t tf_card_read_write_test(void)
{
    static const char test_file[] = BSP_SD_MOUNT_POINT "/test.txt";
    static const char test_data[] =
        "ESP32-C5 TF card read/write test\r\n"
        "Write and read verification passed.\r\n";
    char read_buffer[sizeof(test_data)] = {0};

    ESP_LOGI(TAG, "Starting TF card read/write test");
    ESP_LOGI(TAG, "Test file: %s", test_file);

    FILE *file = fopen(test_file, "wb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: errno=%d (%s)",
                 errno, strerror(errno));
        return ESP_FAIL;
    }

    size_t expected_size = strlen(test_data);
    size_t written_size = fwrite(test_data, 1, expected_size, file);
    if (written_size != expected_size) {
        ESP_LOGE(TAG, "Write failed: expected %u bytes, wrote %u bytes",
                 (unsigned int)expected_size, (unsigned int)written_size);
        fclose(file);
        return ESP_FAIL;
    }

    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "Failed to close file after writing: errno=%d (%s)",
                 errno, strerror(errno));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Write success: %u bytes", (unsigned int)written_size);

    file = fopen(test_file, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: errno=%d (%s)",
                 errno, strerror(errno));
        return ESP_FAIL;
    }

    size_t read_size = fread(read_buffer, 1, expected_size, file);
    bool read_error = ferror(file);
    fclose(file);

    if (read_error || read_size != expected_size) {
        ESP_LOGE(TAG, "Read failed: expected %u bytes, read %u bytes",
                 (unsigned int)expected_size, (unsigned int)read_size);
        return ESP_FAIL;
    }

    if (memcmp(read_buffer, test_data, expected_size) != 0) {
        ESP_LOGE(TAG, "Data verification failed: read data differs from written data");
        return ESP_FAIL;
    }

    read_buffer[read_size] = '\0';
    ESP_LOGI(TAG, "Read success: %u bytes", (unsigned int)read_size);
    ESP_LOGI(TAG, "Read content:\n%s", read_buffer);
    ESP_LOGI(TAG, "========== TF CARD TEST PASS ==========");
    return ESP_OK;
}

void app_main(void)
{
    gpio_reset_pin(BSP_LCD_CS);
    gpio_set_level(BSP_LCD_CS, 1);
    gpio_set_direction(BSP_LCD_CS, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "LCD CS GPIO%d set high", BSP_LCD_CS);

    esp_err_t ret = bsp_sdcard_mount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TF card mount failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "========== TF CARD TEST FAIL ==========");
        return;
    }

    ESP_LOGI(TAG, "TF card mounted at %s", BSP_SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, bsp_sdcard_get_handle());

    ret = tf_card_read_write_test();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "========== TF CARD TEST FAIL ==========");
    }
}
