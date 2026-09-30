#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "dirent.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_spiffs.h"
#include <time.h>
#include <sys/time.h>

#include "bsp/esp32_c5_touch_lcd_3_5.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_st7796.h"

#include "esp_lcd_touch_ft6336.h"

#include "esp_vfs_fat.h"
#include "esp_codec_dev_defaults.h"
#include "bsp_err_check.h"
#include "iot_button.h"
#include "sdmmc_cmd.h"

static const char *TAG = "esp32_c5_touch_lcd_3_5";

/**
 * @brief I2C handle for BSP usage
 *
 * In IDF v5.4 you can call i2c_master_get_bus_handle(BSP_I2C_NUM, i2c_master_bus_handle_t *ret_handle)
 * from #include "esp_private/i2c_platform.h" to get this handle
 *
 * For IDF 5.2 and 5.3 you must call bsp_i2c_get_handle()
 */
static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;
static bool spi_sd_initialized = false;
static sdmmc_card_t *bsp_sdcard = NULL;    // Global uSD card handler
static esp_lcd_touch_handle_t tp;   // LCD touch handle
static esp_lcd_panel_handle_t panel_handle = NULL; // LCD panel handle
static esp_lcd_panel_io_handle_t io_handle = NULL;
#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static lv_indev_t *disp_indev = NULL;
static lv_display_t *disp_drv = NULL;
#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static SemaphoreHandle_t touch_mux;
static uint8_t brightness;
static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *i2s_data_if = NULL;  /* Codec data interface */
static esp_io_expander_handle_t io_expander = NULL;
static qmi8658_dev_t *_qmi8658_dev = NULL; 
static pcf85063a_dev_t *_pcf85063a_dev = NULL;
static i2c_master_dev_handle_t _shtc3_handle = NULL;

/* Can be used for i2s_std_gpio_config_t and/or i2s_std_config_t initialization */

/**
 * @brief Kaluga-kit I2S pinout
 *
 * Can be used for i2s_std_gpio_config_t and/or i2s_std_config_t initialization
 */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

/**
 * @brief Mono Duplex I2S configuration structure
 *
 * This configuration is used by default in bsp_audio_init()
 */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }
esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_config, &i2c_handle));

    i2c_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}

static esp_err_t bsp_spi_init(void)
{
    /* SPI was initialized before */
    if (spi_sd_initialized)
    {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_DATA0,
        .miso_io_num = BSP_SD_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");
    spi_sd_initialized = true;

    return ESP_OK;
}

static esp_err_t bsp_spi_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(spi_bus_free(BSP_LCD_SPI_NUM));
    spi_sd_initialized = false;
    return ESP_OK;
}


void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = slot;

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    assert(config);
    memset(config, 0, sizeof(sdspi_device_config_t));

    config->gpio_cs   = BSP_SD_SPI_CS;
    config->gpio_cd   = SDSPI_SLOT_NO_CD;
    config->gpio_wp   = SDSPI_SLOT_NO_WP;
    config->gpio_int  = GPIO_NUM_NC;
    config->host_id = spi_host;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    config->gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW;
#endif
}


esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdspi_device_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    assert(cfg);

    if(!spi_sd_initialized)
    {
        bsp_spi_init();
    }
    spi_sd_initialized = true;

    if (!cfg->mount) {
        cfg->mount = &mount_config;
    }

    if (!cfg->host) {
        bsp_sdcard_get_sdspi_host(SDSPI_DEFAULT_HOST, &sdhost);
        cfg->host = &sdhost;
    }

    if (!cfg->slot.sdspi) {
        bsp_sdcard_sdspi_get_slot(BSP_LCD_SPI_NUM, &sdslot);
        cfg->slot.sdspi = &sdslot;
    }

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdspi, cfg->mount, &bsp_sdcard),
                        TAG, "SD card SPI mount failed, please check JP8. Pin 2 must be switched to ON.");

    sdmmc_card_print_info(stdout, bsp_sdcard);
    return ESP_OK;
}

esp_err_t bsp_sdcard_mount(void)
{
    bsp_sdcard_cfg_t cfg = {0};
    return bsp_sdcard_sdspi_mount(&cfg);
}

esp_err_t bsp_sdcard_unmount(void)
{
    esp_err_t ret = ESP_OK;

    ret |= esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;

    return ret;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (i2s_data_if) {
        /* Audio was initialized before */
        return ESP_OK;
    }

    i2s_chan_handle_t tx_channel, rx_channel;
    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &tx_channel, &rx_channel));

    /* Setup I2S channels */
    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    if (tx_channel != NULL) {
        BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_init_std_mode(tx_channel, p_i2s_cfg));
        BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_enable(tx_channel));
    }
    if (rx_channel != NULL) {
        BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_init_std_mode(rx_channel, p_i2s_cfg));
        BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_enable(rx_channel));
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_channel,
        .tx_handle = tx_channel,
    };

    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK(i2s_data_if, ESP_FAIL);
    return ESP_OK;
}

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void)
{
    return i2s_data_if;
}


/**
 * @brief Common codec init
 *
 * Kaluga kit uses one codec for both audio playback and recording
 *
 * @return esp_codec_dev_t
 */
static esp_codec_dev_handle_t bsp_audio_codec_init(void)
{
    static esp_codec_dev_handle_t codec = NULL;
    static const audio_codec_data_if_t *i2s_data_if = NULL;  /* Codec data interface */

    // This function can be called only once
    if (NULL != codec) {
        return codec;
    }

    /* Initialize I2S: IDF-version dependant implementation */
    BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    i2s_data_if = bsp_audio_get_codec_itf();
    BSP_NULL_CHECK(i2s_data_if, NULL);

    /* Initialize I2C */
    bsp_i2c_init();
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    /* Create new ES8311 codec */
    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = false,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    codec = esp_codec_dev_new(&codec_dev_cfg);
    BSP_NULL_CHECK(codec, NULL);
    return codec;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    return bsp_audio_codec_init();
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    return bsp_audio_codec_init();
}


esp_err_t bsp_io_expander_init(void)
{
    esp_err_t ret = ESP_OK;
    bsp_i2c_init();
    ret = custom_io_expander_new_i2c_ch32v003(i2c_handle, BSP_IO_EXPANDER_I2C_ADDRESS_CH32V003, &io_expander);
    esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_5, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_INPUT);
    esp_io_expander_set_level(io_expander,   IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 , 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    esp_io_expander_set_level(io_expander,   IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 , 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    esp_io_expander_set_level(io_expander,   IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_5 , 1);
    return ret;
}

esp_io_expander_handle_t bsp_get_io_expander(void)
{
    return io_expander;
}

uint16_t bsp_get_io_expander_adc(void)
{
    uint16_t adc_value;
    custom_io_expander_get_adc(io_expander, &adc_value);
    return adc_value;
}

// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;

    if(!spi_sd_initialized)
    {
        bsp_spi_init();
    }
    spi_sd_initialized = true;

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST, // Shared with Touch reset
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };

    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(*ret_io, &panel_config, ret_panel));

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
    esp_lcd_panel_invert_color(*ret_panel, true);
    esp_lcd_panel_disp_on_off(*ret_panel, true);

    esp_lcd_panel_mirror(*ret_panel,true,false);

    return ret;

err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


esp_err_t bsp_touch_new(const bsp_display_cfg_t *cfg, esp_lcd_touch_handle_t *ret_touch)
{
    assert(cfg != NULL);
    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());


    touch_mux = xSemaphoreCreateBinary();
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_get_bus_handle(BSP_I2C_NUM,  &i2c_handle);

    /* Initialize touch HW */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,
        .y_max = BSP_LCD_H_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST,
        .int_gpio_num = BSP_LCD_TOUCH_INT, 
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = cfg->touch_flags.swap_xy,
            .mirror_x = cfg->touch_flags.mirror_x,
            .mirror_y = cfg->touch_flags.mirror_y,
        },
    };

    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;
    tp_cfg.x_max = BSP_LCD_H_RES;
    tp_cfg.x_max = BSP_LCD_V_RES;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT6336_CONFIG();
    esp_lcd_new_panel_io_i2c((i2c_master_bus_handle_t)i2c_handle, &tp_io_config, &tp_io_handle);
    return esp_lcd_touch_new_i2c_ft6336(tp_io_handle, &tp_cfg, ret_touch);



}


#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);

    bsp_display_config_t disp_config = {0};
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&disp_config, &panel_handle, &io_handle));

    ESP_LOGD(TAG, "Add LCD screen");
    esp_lv_adapter_display_config_t disp_cfg = {
        .panel = panel_handle,
        .panel_io = io_handle,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = cfg->rotation,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 80,
            .use_psram = true,
            .enable_ppa_accel = false,
            .require_double_buffer = true,
        },
        .tear_avoid_mode = cfg->tear_avoid_mode,
    };

    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    if (!disp)
    {
        return NULL;
    }

    return disp;
}

static lv_indev_t *bsp_display_indev_init(const bsp_display_cfg_t *cfg, lv_display_t *disp)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(cfg, &tp));
    assert(tp);

    const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);

    return esp_lv_adapter_register_touch(&touch_cfg);
}
#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)


esp_err_t bsp_display_brightness_init(void)
{
    ESP_LOGW(TAG, "This board doesn't support to change brightness of LCD");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    } else if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    int flipped_brightness = brightness_percent;

    brightness = (uint8_t)((flipped_brightness * 255) / 100);

    ESP_LOGI(TAG, "Setting flipped LCD backlight: %d%% (original: %d%%)", flipped_brightness, brightness_percent);

    custom_io_expander_set_pwm(io_expander,(uint8_t)brightness);
    return ESP_OK;
}

int bsp_display_brightness_get(void)
{
    if (disp_drv == NULL)
    {
        ESP_LOGE(TAG, "disp_drv is not initialized");
        return -1;
    }

    return brightness * 100 / 255;
}

esp_err_t bsp_display_backlight_off(void)
{
    ESP_LOGI(TAG, "Backlight off");
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    ESP_LOGI(TAG, "Backlight on");
    return bsp_display_brightness_set(100);
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
        .touch_flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0}};
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(bsp_display_cfg_t *cfg)
{
    lv_display_t *disp;

    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_init(&cfg->lv_adapter_cfg));

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(cfg, disp), NULL);

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    disp_drv = disp;
    return disp;
}

lv_display_t *bsp_display_get_disp_dev(void)
{
    return disp_drv;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

esp_lcd_touch_handle_t bsp_display_get_touch_handle(void)
{
    return tp;
}

esp_err_t bsp_display_lock(uint32_t timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/**
 * @brief A universal function to retrieve a list of files with specific extensions in a designated directory
 * 
 * @param dir_path path (example "/sdcard")
 * @param extension target extension (example ".jpg", ".avi", ".png")
 * @param out 
 * @return esp_err_t 
 */
esp_err_t get_file_list_by_ext(const char *dir_path, const char *extension, generic_file_list_t *out)
{
    if (!dir_path || !extension || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    out->list  = NULL;
    out->count = 0;

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return ESP_FAIL;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { 
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, extension) == 0) {
                count++;
            }
        }
    }

    if (count == 0) {
        closedir(dir);
        return ESP_ERR_NOT_FOUND;
    }

    char **list = (char **)malloc(sizeof(char *) * count);
    if (!list) {
        closedir(dir);
        return ESP_ERR_NO_MEM;
    }

    rewinddir(dir);
    int idx = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, extension) == 0) {

                size_t len = strlen(dir_path) + strlen(entry->d_name) + 2;
                char *full_path = (char *)malloc(len);
                if (!full_path) {
 
                    for (int i = 0; i < idx; i++) free(list[i]);
                    free(list);
                    closedir(dir);
                    return ESP_ERR_NO_MEM;
                }
                snprintf(full_path, len, "%s/%s", dir_path, entry->d_name);
                list[idx++] = full_path;
            }
        }
    }

    closedir(dir);
    out->list  = list;
    out->count = idx;
    return ESP_OK;
}

qmi8658_dev_t *bsp_qmi8658_drv_init(void)
{
    if(_qmi8658_dev != NULL)
    {
        return _qmi8658_dev;
    }
    if (bsp_i2c_init() != ESP_OK) {
        return NULL;
    }

    uint8_t address = QMI8658_ADDRESS_HIGH;
    esp_err_t ret = i2c_master_probe(i2c_handle, address, 100);
    if (ret != ESP_OK) {
        address = QMI8658_ADDRESS_LOW;
        ret = i2c_master_probe(i2c_handle, address, 100);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 not found at 0x%02X or 0x%02X",
                 QMI8658_ADDRESS_HIGH, QMI8658_ADDRESS_LOW);
        return NULL;
    }

    ESP_LOGI(TAG, "QMI8658 detected at I2C address 0x%02X", address);
    _qmi8658_dev = (qmi8658_dev_t *)calloc(sizeof(qmi8658_dev_t),1);
    if (_qmi8658_dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate QMI8658 device");
        return NULL;
    }

    ret = qmi8658_init(_qmi8658_dev, i2c_handle, address);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize QMI8658: %s", esp_err_to_name(ret));
        if (_qmi8658_dev->dev_handle != NULL) {
            i2c_master_bus_rm_device(_qmi8658_dev->dev_handle);
        }
        free(_qmi8658_dev);
        _qmi8658_dev = NULL;
        return NULL;
    }
    
    qmi8658_set_accel_range(_qmi8658_dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(_qmi8658_dev, QMI8658_ACCEL_ODR_500HZ);
    qmi8658_set_accel_unit_mps2(_qmi8658_dev, true);
    qmi8658_write_register(_qmi8658_dev, QMI8658_CTRL5, 0x03);

    return _qmi8658_dev;
}


pcf85063a_dev_t *bsp_pcf85063a_drv_init(void)
{
    if(_pcf85063a_dev != NULL)
    {
        return _pcf85063a_dev;
    }
    bsp_i2c_init();
    _pcf85063a_dev = (pcf85063a_dev_t *)calloc(sizeof(pcf85063a_dev_t),1);


    ESP_ERROR_CHECK(pcf85063a_init(_pcf85063a_dev, i2c_handle, PCF85063A_ADDRESS));

    return _pcf85063a_dev;
}


i2c_master_dev_handle_t bsp_shtc3_drv_init(void)
{
    if(_shtc3_handle != NULL)
    {
        return _shtc3_handle;
    }
    bsp_i2c_init();

    _shtc3_handle = shtc3_device_create(i2c_handle, SHTC3_I2C_ADDR, CONFIG_SHTC3_I2C_CLK_SPEED_HZ);

    uint8_t sensor_id[2];
    esp_err_t err = shtc3_get_id(_shtc3_handle, sensor_id);
    ESP_LOGI(TAG, "Sensor ID: 0x%02x%02x", sensor_id[0], sensor_id[1]);

    return _shtc3_handle;
}

void sync_system_time_from_rtc(pcf85063a_datetime_t *rtc_time)
{
    struct tm t_st = {0};

    // 将 RTC 结构体转换为 struct tm
    t_st.tm_year  = rtc_time->year - 1900;  // tm_year 是从1900年起的偏移
    t_st.tm_mon   = rtc_time->month - 1;    // tm_mon 范围 0-11
    t_st.tm_mday  = rtc_time->day;
    t_st.tm_hour  = rtc_time->hour;
    t_st.tm_min   = rtc_time->min;
    t_st.tm_sec   = rtc_time->sec;
    t_st.tm_wday  = rtc_time->dotw;         // 0=Sunday
    t_st.tm_isdst = -1;

    // 设置 TZ 为 UTC，确保 mktime() 按 UTC 解析（避免时区影响）
    setenv("TZ", "GMT0", 1);
    tzset();

    time_t t = mktime(&t_st);

    struct timeval now = {
        .tv_sec  = t,
        .tv_usec = 0
    };
    settimeofday(&now, NULL);

    // 恢复你需要的本地时区，例如 CST-8
    setenv("TZ", "CST-8", 1);
    tzset();
}
