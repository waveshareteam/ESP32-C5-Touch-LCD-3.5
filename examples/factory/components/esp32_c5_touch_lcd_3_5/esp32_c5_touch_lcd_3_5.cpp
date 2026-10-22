#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "bsp_board_internal.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <cstring>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_codec_dev_defaults.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft6336.h"
#include "driver/ledc.h"
#include "driver/sdspi_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "bsp_c5_3_5";
static i2c_master_bus_handle_t i2c_bus;
static esp_lcd_panel_io_handle_t lcd_io, touch_io;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static uint8_t last_touch_gesture = 0xff;
static lv_display_t *display;
static lv_indev_t *input;
static bool backlight_ready, display_started, port_initialized;
// Settings may restore the desired brightness before the desktop is drawn.
// Keep the PWM off until bsp_display_show() has flushed the first UI frame.
static bool backlight_released;
static int brightness = CONFIG_BSP_LCD_BRIGHTNESS;
static SemaphoreHandle_t sd_mutex;
static sdmmc_card_t *sdcard;
static unsigned sd_leases;
static bool spi_ready;
static SemaphoreHandle_t spi_mutex;
static constexpr size_t LCD_DMA_BYTES = BSP_LCD_H_RES * 10 * sizeof(uint16_t);
static uint8_t *lcd_dma_buffer;
// Protected by spi_mutex, including LVGL flushes and camera transfers.
static bool preview_requested, preview_active;
static lv_area_t preview_area;

/**************************************************************************************************
 * I2C interface
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void)
{
    if (i2c_bus) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_board_init_locks(), TAG, "Board locks");
    if (!sd_mutex) sd_mutex = xSemaphoreCreateMutex();
    if (!sd_mutex) return ESP_ERR_NO_MEM;
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = BSP_I2C_NUM;
    cfg.sda_io_num = BSP_I2C_SDA;
    cfg.scl_io_num = BSP_I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&cfg, &i2c_bus);
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!i2c_bus) return ESP_OK;
    // Child devices must be removed before deleting their common bus.
    if (touch || bsp_board_i2c_in_use()) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = i2c_del_master_bus(i2c_bus);
    if (ret == ESP_OK) i2c_bus = nullptr;
    return ret;
}
i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return bsp_i2c_init() == ESP_OK ? i2c_bus : nullptr;
}

static esp_err_t bsp_spi_init(int max_transfer_size)
{
    if (spi_ready) return ESP_OK;
    if (!spi_mutex) spi_mutex = xSemaphoreCreateMutex();
    if (!spi_mutex) return ESP_ERR_NO_MEM;
    gpio_set_level(BSP_LCD_CS, 1);
    gpio_set_direction(BSP_LCD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_SD_CS, 1);
    gpio_set_direction(BSP_SD_CS, GPIO_MODE_OUTPUT);
    spi_bus_config_t cfg = {};
    cfg.sclk_io_num = BSP_LCD_SCLK;
    cfg.mosi_io_num = BSP_LCD_MOSI;
    cfg.miso_io_num = BSP_SD_MISO;
    cfg.quadwp_io_num = cfg.quadhd_io_num = -1;
    cfg.max_transfer_sz = max_transfer_size;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_HOST, &cfg, SPI_DMA_CH_AUTO), TAG, "LCD/SD SPI");
    spi_ready = true;
    return ESP_OK;
}

static esp_err_t bsp_sdspi_do_transaction(int slot, sdmmc_command_t *command)
{
    // SD workers must not take the LVGL lock: APP closure waits for them while
    // already holding that lock. Use the same SPI mutex as the flush events.
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    esp_err_t ret = sdspi_host_do_transaction(slot, command);
    xSemaphoreGive(spi_mutex);
    return ret;
}

static void bsp_display_spi_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_FLUSH_START) {
        xSemaphoreTake(spi_mutex, portMAX_DELAY);
    } else {
        // FLUSH_FINISH means submitted, not DMA-complete. Drain LCD transfers
        // before letting SDSPI polling reuse the shared SPI2 peripheral.
        const esp_err_t ret = esp_lcd_panel_io_tx_param(lcd_io, -1, nullptr, 0);
        if (ret != ESP_OK) ESP_LOGE(TAG, "LCD transfer drain failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(spi_mutex);
    }
}

static esp_err_t sdcard_mount_locked(void)
{
    if (sdcard) return ESP_OK;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed = false;
    cfg.max_files = 8;
    cfg.allocation_unit_size = 16 * 1024;
    ESP_RETURN_ON_ERROR(bsp_spi_init(BSP_LCD_H_RES * CONFIG_BSP_LCD_BUFFER_HEIGHT * 2), TAG, "Shared SPI");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BSP_LCD_SPI_HOST;
    host.do_transaction = bsp_sdspi_do_transaction;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = BSP_LCD_SPI_HOST;
    slot.gpio_cs = BSP_SD_CS;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, &host, &slot, &cfg, &sdcard);
    if (ret != ESP_OK) sdcard = nullptr;
    bsp_board_status.sdcard = ret;
    return ret;
}

esp_err_t bsp_sdcard_mount(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sdcard_mount_locked();
    xSemaphoreGive(sd_mutex);
    return ret;
}

esp_err_t bsp_sdcard_acquire(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sdcard_mount_locked();
    if (ret == ESP_OK) ++sd_leases;
    xSemaphoreGive(sd_mutex);
    return ret;
}

void bsp_sdcard_release(void)
{
    if (!sd_mutex) return;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    if (sd_leases) --sd_leases;
    xSemaphoreGive(sd_mutex);
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sd_leases ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (ret == ESP_OK && sdcard) {
        ret = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, sdcard);
        if (ret == ESP_OK) { sdcard = nullptr; bsp_board_status.sdcard = ESP_ERR_INVALID_STATE; }
    }
    xSemaphoreGive(sd_mutex);
    return ret;
}

esp_err_t bsp_sdcard_get_info(bsp_sdcard_info_t *info)
{
    if (!info) return ESP_ERR_INVALID_ARG;
    *info = {};
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (sdcard) {
        info->mounted = true;
        info->capacity_bytes = (uint64_t)sdcard->csd.capacity * sdcard->csd.sector_size;
        ret = esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &info->total_bytes, &info->free_bytes);
    }
    xSemaphoreGive(sd_mutex);
    return ret;
}


/**************************************************************************************************
 * I2S audio interface: one shared ES8311 for playback and microphone capture.
 **************************************************************************************************/
static i2s_chan_handle_t tx, rx;
static const audio_codec_data_if_t *data_if;
static const audio_codec_ctrl_if_t *ctrl_if;
static const audio_codec_gpio_if_t *gpio_if;
static const audio_codec_if_t *codec_if;
static esp_codec_dev_handle_t codec;

static void bsp_audio_cleanup(void)
{
    if (codec) { esp_codec_dev_delete(codec); codec = nullptr; }
    if (codec_if) { audio_codec_delete_codec_if(codec_if); codec_if = nullptr; }
    if (ctrl_if) { audio_codec_delete_ctrl_if(ctrl_if); ctrl_if = nullptr; }
    if (gpio_if) { audio_codec_delete_gpio_if(gpio_if); gpio_if = nullptr; }
    if (data_if) { audio_codec_delete_data_if(data_if); data_if = nullptr; }
    if (tx) { i2s_channel_disable(tx); i2s_del_channel(tx); tx = nullptr; }
    if (rx) { i2s_channel_disable(rx); i2s_del_channel(rx); rx = nullptr; }
}

esp_err_t bsp_audio_deinit(void)
{
    // All APP workers must be stopped before releasing these BSP-owned handles.
    bsp_audio_cleanup();
    return ESP_OK;
}

static esp_err_t bsp_audio_init_channels(const i2s_std_config_t *i2s_config)
{
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = BSP_AUDIO_DMA_DESCRIPTORS;
    channel.dma_frame_num = BSP_AUDIO_DMA_FRAMES;
    channel.auto_clear_after_cb = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, &tx, &rx), TAG, "I2S channels");
    i2s_std_config_t cfg = {};
    if (i2s_config) cfg = *i2s_config;
    else {
        cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        cfg.gpio_cfg.mclk = BSP_I2S_MCLK;
        cfg.gpio_cfg.bclk = BSP_I2S_BCLK;
        cfg.gpio_cfg.ws = BSP_I2S_LRCLK;
        cfg.gpio_cfg.dout = BSP_I2S_DOUT;
        cfg.gpio_cfg.din = BSP_I2S_DIN;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx, &cfg), TAG, "I2S TX");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx, &cfg), TAG, "I2S RX");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx), TAG, "I2S TX enable");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(rx), TAG, "I2S RX enable");
    audio_codec_i2s_cfg_t data = {};
    data.port = I2S_NUM_0;
    data.tx_handle = tx;
    data.rx_handle = rx;
    data_if = audio_codec_new_i2s_data(&data);
    return data_if ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (data_if) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C bus");
    esp_err_t ret = bsp_audio_init_channels(i2s_config);
    if (ret != ESP_OK) bsp_audio_cleanup();
    return ret;
}

esp_err_t bsp_audio_init_voice_24k(void)
{
    i2s_std_config_t cfg = {};
    cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000);
    cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    cfg.gpio_cfg.mclk = BSP_I2S_MCLK;
    cfg.gpio_cfg.bclk = BSP_I2S_BCLK;
    cfg.gpio_cfg.ws = BSP_I2S_LRCLK;
    cfg.gpio_cfg.dout = BSP_I2S_DOUT;
    cfg.gpio_cfg.din = BSP_I2S_DIN;
    return bsp_audio_init(&cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (codec) return codec;
    if (bsp_audio_init(nullptr) != ESP_OK) return nullptr;
    // Codec control uses an 8-bit address; the bus probe takes 7 bits.
    if (i2c_master_probe(bsp_i2c_get_handle(), ES8311_CODEC_DEFAULT_ADDR >> 1, 100) != ESP_OK) return nullptr;
    audio_codec_i2c_cfg_t ctrl = {};
    ctrl.port = BSP_I2C_NUM;
    ctrl.addr = ES8311_CODEC_DEFAULT_ADDR;
    ctrl.bus_handle = bsp_i2c_get_handle();
    ctrl_if = audio_codec_new_i2c_ctrl(&ctrl);
    gpio_if = audio_codec_new_gpio();
    if (!ctrl_if || !gpio_if) goto failed;
    {
        es8311_codec_cfg_t chip = {};
        chip.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
        chip.ctrl_if = ctrl_if;
        chip.gpio_if = gpio_if;
        chip.pa_pin = BSP_AUDIO_PA;
        chip.use_mclk = false;
        chip.hw_gain.pa_voltage = 5.0f;
        chip.hw_gain.codec_dac_voltage = 3.3f;
        codec_if = es8311_codec_new(&chip);
        if (!codec_if) goto failed;
        esp_codec_dev_cfg_t dev = {};
        dev.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT;
        dev.codec_if = codec_if;
        dev.data_if = data_if;
        codec = esp_codec_dev_new(&dev);
        if (!codec) goto failed;
    }
    return codec;
failed:
    bsp_audio_cleanup();
    return nullptr;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    // Both directions belong to the same physical ES8311. Never delete twice.
    return bsp_audio_codec_speaker_init();
}

// Board extensions retain bounded raw I2S transfers for the voice workers.
esp_err_t bsp_audio_i2s_read(void *data, size_t size, size_t *read, uint32_t timeout_ms)
{
    return rx ? i2s_channel_read(rx, data, size, read, timeout_ms) : ESP_ERR_INVALID_STATE;
}
esp_err_t bsp_audio_i2s_write(void *data, size_t size, size_t *written, uint32_t timeout_ms)
{
    return tx ? i2s_channel_write(tx, data, size, written, timeout_ms) : ESP_ERR_INVALID_STATE;
}

esp_err_t bsp_display_brightness_init(void)
{
    if (backlight_ready) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_io_expander_init(), TAG, "Backlight expander");
    ESP_RETURN_ON_ERROR(bsp_board_backlight_set(0), TAG, "Backlight off");
    backlight_ready = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int percent)
{
    if (percent < 0 || percent > 100) return ESP_ERR_INVALID_ARG;
    if (!backlight_ready) return ESP_ERR_INVALID_STATE;
    const uint8_t duty = backlight_released ? percent * 255 / 100 : 0;
    ESP_RETURN_ON_ERROR(bsp_board_backlight_set(duty), TAG, "Backlight PWM");
    brightness = percent;
    return ESP_OK;
}
int bsp_display_brightness_get(void) { return brightness; }
esp_err_t bsp_display_backlight_off(void) { return bsp_display_brightness_set(0); }
esp_err_t bsp_display_backlight_on(void) { return bsp_display_brightness_set(CONFIG_BSP_LCD_BRIGHTNESS); }

esp_err_t bsp_display_show(void)
{
    if (!display_started || !display || !lcd_io) return ESP_ERR_INVALID_STATE;
    if (!lvgl_port_lock(0)) return ESP_ERR_TIMEOUT;
    lv_obj_invalidate(lv_display_get_screen_active(display));
    lv_refr_now(display);
    esp_err_t ret = esp_lcd_panel_io_tx_param(lcd_io, -1, nullptr, 0);
    if (ret == ESP_OK) {
        backlight_released = true;
        ret = bsp_display_brightness_set(brightness);
        if (ret != ESP_OK) backlight_released = false;
    }
    lvgl_port_unlock();
    if (ret == ESP_OK) ESP_LOGI(TAG, "Initial UI frame flushed; backlight enabled at %d%%", brightness);
    return ret;
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    if (!ret_panel || !ret_io) return ESP_ERR_INVALID_ARG;
    if (panel || lcd_io) return ESP_ERR_INVALID_STATE;
    *ret_panel = nullptr;
    *ret_io = nullptr;
    esp_lcd_panel_handle_t new_panel = nullptr;
    esp_lcd_panel_io_handle_t new_io = nullptr;
    bool bus_created = false;
    auto create = [&]() -> esp_err_t {
        ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Backlight");
        ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
        ESP_RETURN_ON_ERROR(bsp_io_expander_init(), TAG, "Expander");
        ESP_RETURN_ON_ERROR(bsp_power_init(), TAG, "Power");
        const int transfer = config && config->max_transfer_sz > 0 ? config->max_transfer_sz : BSP_LCD_H_RES * CONFIG_BSP_LCD_BUFFER_HEIGHT * 2;
        ESP_RETURN_ON_ERROR(bsp_spi_init(transfer), TAG, "LCD SPI");
        bus_created = true;
        esp_lcd_panel_io_spi_config_t io = {};
        io.cs_gpio_num = BSP_LCD_CS;
        io.dc_gpio_num = BSP_LCD_DC;
        io.spi_mode = 0;
        io.pclk_hz = CONFIG_BSP_LCD_SPI_CLOCK_HZ;
        io.trans_queue_depth = 10;
        io.lcd_cmd_bits = 8;
        io.lcd_param_bits = 8;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST, &io, &new_io), TAG, "LCD IO");
    #if CONFIG_BSP_LCD_POWERON_RESTART
        if (esp_reset_reason() == ESP_RST_POWERON) {
            ESP_LOGI(TAG, "Original LCD cold-start workaround: restarting once");
            esp_restart();
        }
    #endif
        esp_lcd_panel_dev_config_t cfg = {};
        cfg.reset_gpio_num = BSP_LCD_RST;
        cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        cfg.bits_per_pixel = 16;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(new_io, &cfg, &new_panel), TAG, "ST7796 create");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(new_panel), TAG, "LCD reset");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_init(new_panel), TAG, "LCD init");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(new_panel, true), TAG, "LCD inversion");
        // Native portrait orientation from the original rotation=0 configuration.
        ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(new_panel, false), TAG, "LCD axes");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(new_panel, true, false), TAG, "LCD mirror");

        return ESP_OK;
    };
    esp_err_t ret = create();
    if (ret != ESP_OK) {
        if (new_panel) esp_lcd_panel_del(new_panel);
        if (new_io) esp_lcd_panel_io_del(new_io);
        if (bus_created && !sdcard) { spi_bus_free(BSP_LCD_SPI_HOST); spi_ready = false; }
        return ret;
    }
    *ret_panel = new_panel;
    *ret_io = new_io;
    return ESP_OK;
}
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;
    if (!ret_touch) return ESP_ERR_INVALID_ARG;
    if (touch || touch_io) return ESP_ERR_INVALID_STATE;
    *ret_touch = nullptr;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
    ESP_RETURN_ON_ERROR(bsp_io_expander_init(), TAG, "Expander");
    esp_lcd_panel_io_i2c_config_t io = {};
    io.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT6336_ADDRESS;
    io.control_phase_bytes = 1;
    io.lcd_cmd_bits = 8;
    io.flags.disable_control_phase = true;
    io.scl_speed_hz = BSP_I2C_FREQ_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus, &io, &touch_io), TAG, "Touch IO");
    esp_lcd_touch_config_t cfg = {};
    cfg.x_max = BSP_LCD_H_RES;
    cfg.y_max = BSP_LCD_V_RES;
    cfg.rst_gpio_num = BSP_TOUCH_RST;
    cfg.int_gpio_num = BSP_TOUCH_INT;
    esp_err_t ret = esp_lcd_touch_new_i2c_ft6336(touch_io, &cfg, &touch);
    if (ret != ESP_OK) {
        touch = nullptr;
        esp_lcd_panel_io_del(touch_io);
        touch_io = nullptr;
        return ret;
    }
    // Factory-only double-tap mode ends before normal UI in the old project.
    // LVGL/Brookesia swipe gestures still work with normal coordinate reporting.
    ret = esp_lcd_touch_ft6336_set_gesture_enabled(touch, false);
    if (ret != ESP_OK) {
        esp_lcd_touch_del(touch); touch = nullptr;
        esp_lcd_panel_io_del(touch_io); touch_io = nullptr;
        return ret;
    }
    *ret_touch = touch;
    return ESP_OK;
}

esp_err_t bsp_touch_set_gesture_enabled(bool enabled)
{
    if (!touch) return ESP_ERR_INVALID_STATE;
    if (enabled) {
        // D0 off/on does not clear the last D3 result on this panel.
        ESP_RETURN_ON_ERROR(esp_lcd_touch_ft6336_set_gesture_enabled(touch, false),
                            TAG, "Reset touch gesture mode");
        ESP_RETURN_ON_ERROR(esp_lcd_touch_ft6336_clear_gesture(touch),
                            TAG, "Clear previous touch gesture before entry");
    }
    ESP_RETURN_ON_ERROR(esp_lcd_touch_ft6336_set_gesture_enabled(touch, enabled),
                        TAG, "Touch gesture mode");
    if (enabled) {
        // Verify the result is still clear after enabling. Do not start an APP
        // reader if the chip retained a previous double-click result.
        const esp_err_t ret = esp_lcd_touch_ft6336_clear_gesture(touch);
        if (ret != ESP_OK) {
            esp_lcd_touch_ft6336_set_gesture_enabled(touch, false);
            return ret;
        }
        ESP_LOGI(TAG, "Touch gesture cleared and verified: 0xD3=0x00");
    }
    last_touch_gesture = 0xff;
    ESP_LOGI(TAG, "Touch hardware gesture mode %s (0xD0=%u)",
             enabled ? "enabled" : "disabled", enabled ? 1U : 0U);
    return ESP_OK;
}

esp_err_t bsp_touch_get_double_click(bool *detected)
{
    if (!detected) return ESP_ERR_INVALID_ARG;
    *detected = false;
    if (!touch) return ESP_ERR_INVALID_STATE;
    esp_lcd_touch_ft6336_gesture_t gesture;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_ft6336_get_gesture(touch, &gesture),
                        TAG, "Touch gesture read");
    const uint8_t raw = static_cast<uint8_t>(gesture);
    if (raw != last_touch_gesture) {
        ESP_LOGI(TAG, "Touch gesture raw: 0xD3=0x%02X", static_cast<unsigned>(raw));
        last_touch_gesture = raw;
    }
    if (gesture != ESP_LCD_TOUCH_FT6336_GESTURE_NONE) {
        // Consume the hardware result once. A mode switch alone leaves D3
        // latched and would replay this result the next time the APP opens.
        ESP_RETURN_ON_ERROR(esp_lcd_touch_ft6336_clear_gesture(touch),
                            TAG, "Acknowledge touch gesture");
        if (gesture == ESP_LCD_TOUCH_FT6336_GESTURE_DOUBLE_CLICK) {
            ESP_LOGI(TAG, "Double click consumed; 0xD3 cleared and verified");
        }
    }
    *detected = gesture == ESP_LCD_TOUCH_FT6336_GESTURE_DOUBLE_CLICK;
    return ESP_OK;
}

// Caller owns spi_mutex. Bound DMA memory even when draw buffers live in PSRAM.
static esp_err_t bsp_display_write(int x, int y, int width, int height,
                                   const uint8_t *pixels, size_t stride, bool swap_bytes)
{
    const int rows_per_transfer = LCD_DMA_BYTES / (width * 2);
    for (int row = 0; row < height; row += rows_per_transfer) {
        const int rows = LV_MIN(rows_per_transfer, height - row);
        for (int i = 0; i < rows; ++i) {
            const uint8_t *src = pixels + (row + i) * stride;
            uint8_t *dst = lcd_dma_buffer + i * width * 2;
            if (swap_bytes) {
                for (int col = 0; col < width * 2; col += 2) {
                    dst[col] = src[col + 1];
                    dst[col + 1] = src[col];
                }
            } else {
                memcpy(dst, src, width * 2);
            }
        }
        const esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, x, y + row, x + width, y + row + rows, lcd_dma_buffer);
        // Always drain before reusing the staging buffer or returning a frame.
        const esp_err_t drained = esp_lcd_panel_io_tx_param(lcd_io, -1, nullptr, 0);
        if (ret != ESP_OK) return ret;
        if (drained != ESP_OK) return drained;
    }
    return ESP_OK;
}

static void bsp_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixels)
{
    // FLUSH_START already owns spi_mutex. Keep LVGL out of the live preview,
    // while allowing buttons, status text and touch processing to run normally.
    const lv_area_t intersection = {
        LV_MAX(area->x1, preview_area.x1), LV_MAX(area->y1, preview_area.y1),
        LV_MIN(area->x2, preview_area.x2), LV_MIN(area->y2, preview_area.y2)
    };
    lv_area_t parts[4];
    unsigned count = 1;
    parts[0] = *area;
    if (preview_active && intersection.x1 <= intersection.x2 && intersection.y1 <= intersection.y2) {
        parts[0] = {area->x1, area->y1, area->x2, intersection.y1 - 1};
        parts[1] = {area->x1, intersection.y2 + 1, area->x2, area->y2};
        parts[2] = {area->x1, intersection.y1, intersection.x1 - 1, intersection.y2};
        parts[3] = {intersection.x2 + 1, intersection.y1, area->x2, intersection.y2};
        count = 4;
    }
    const size_t stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), LV_COLOR_FORMAT_RGB565);
    esp_err_t ret = ESP_OK;
    for (unsigned i = 0; i < count; ++i) {
        const lv_area_t &part = parts[i];
        if (part.x1 > part.x2 || part.y1 > part.y2) continue;
        const uint8_t *src = pixels + (part.y1 - area->y1) * stride + (part.x1 - area->x1) * 2;
        ret = bsp_display_write(part.x1, part.y1, lv_area_get_width(&part), lv_area_get_height(&part), src, stride, true);
        if (ret != ESP_OK) break;
    }
    if (ret != ESP_OK) ESP_LOGE(TAG, "Display flush failed: %s", esp_err_to_name(ret));
    // Synchronous staging owns completion; camera writes must not signal LVGL.
    lv_display_flush_ready(disp);
}

esp_err_t bsp_display_preview_start(int x, int y, int width, int height)
{
    if (!display || !lcd_dma_buffer) return ESP_ERR_INVALID_STATE;
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > BSP_LCD_H_RES - x || height > BSP_LCD_V_RES - y) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    preview_area = {x, y, x + width - 1, y + height - 1};
    // Let LVGL paint the new APP and its placeholder until a frame is ready.
    preview_requested = true;
    preview_active = false;
    xSemaphoreGive(spi_mutex);
    return ESP_OK;
}

esp_err_t bsp_display_preview_draw(const void *pixels, size_t size)
{
    if (!pixels) return ESP_ERR_INVALID_ARG;
    if (!display || !lcd_dma_buffer) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (preview_requested) {
        const int width = lv_area_get_width(&preview_area), height = lv_area_get_height(&preview_area);
        ret = size < size_t(width) * height * 2 ? ESP_ERR_INVALID_SIZE :
              bsp_display_write(preview_area.x1, preview_area.y1, width, height,
                                static_cast<const uint8_t *>(pixels), width * 2, false);
        // Switch ownership under the SPI lock, so a pending LVGL flush cannot
        // overwrite the first completed frame with the loading placeholder.
        if (ret == ESP_OK) preview_active = true;
    }
    xSemaphoreGive(spi_mutex);
    return ret;
}

void bsp_display_preview_stop(void)
{
    if (!display) return;
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    preview_requested = false;
    preview_active = false;
    xSemaphoreGive(spi_mutex);
    lv_obj_invalidate(lv_display_get_screen_active(display));
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    if (!lcd_dma_buffer) lcd_dma_buffer = static_cast<uint8_t *>(heap_caps_malloc(LCD_DMA_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!lcd_dma_buffer) return nullptr;
    bsp_display_config_t panel_cfg = {};
    panel_cfg.max_transfer_sz = (cfg->trans_size ? cfg->trans_size : cfg->buffer_size) * sizeof(uint16_t);
    if (bsp_display_new(&panel_cfg, &panel, &lcd_io) != ESP_OK) return nullptr;
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = lcd_io;
    disp_cfg.panel_handle = panel;
    disp_cfg.buffer_size = cfg->buffer_size;
    disp_cfg.trans_size = cfg->trans_size;
    disp_cfg.double_buffer = cfg->double_buffer;
    disp_cfg.hres = BSP_LCD_H_RES;
    disp_cfg.vres = BSP_LCD_V_RES;
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.flags.buff_dma = cfg->flags.buff_dma;
    disp_cfg.flags.buff_spiram = cfg->flags.buff_spiram;
    disp_cfg.flags.swap_bytes = true;
    if (!lvgl_port_lock(0)) return nullptr;
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp) {
        const esp_lcd_panel_io_callbacks_t callbacks = {};
        if (esp_lcd_panel_io_register_event_callbacks(lcd_io, &callbacks, nullptr) != ESP_OK ||
            !lv_display_add_event_cb(disp, bsp_display_spi_event, LV_EVENT_FLUSH_START, nullptr) ||
            !lv_display_add_event_cb(disp, bsp_display_spi_event, LV_EVENT_FLUSH_FINISH, nullptr)) {
            lvgl_port_remove_disp(disp);
            disp = nullptr;
        } else {
            lv_display_set_flush_cb(disp, bsp_display_flush);
        }
    }
    lvgl_port_unlock();
    return disp;
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    bsp_board_status.touch = bsp_touch_new(nullptr, &touch);
    if (bsp_board_status.touch != ESP_OK) return nullptr;
    lvgl_port_touch_cfg_t touch_cfg = {};
    touch_cfg.disp = disp;
    touch_cfg.handle = touch;
    return lvgl_port_add_touch(&touch_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {};
    cfg.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    cfg.lvgl_port_cfg.task_stack = 16 * 1024;
    cfg.lvgl_port_cfg.task_max_sleep_ms = 20;
    cfg.buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_BUFFER_HEIGHT;
    cfg.double_buffer = true;
    cfg.flags.buff_spiram = true;
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    if (display_started) return display;
    if (!cfg || !cfg->buffer_size) return nullptr;
    bsp_board_status.display = bsp_display_brightness_init();
    if (bsp_board_status.display != ESP_OK) return nullptr;
    bsp_board_status.i2c = bsp_i2c_init();
    if (bsp_board_status.i2c != ESP_OK) return nullptr;
    bsp_board_status.expander = bsp_io_expander_init();
    if (bsp_board_status.expander != ESP_OK) return nullptr;
    bsp_board_status.power = bsp_power_init();
    if (bsp_board_status.power != ESP_OK) return nullptr;
    bsp_board_status.display = lvgl_port_init(&cfg->lvgl_port_cfg);
    if (bsp_board_status.display != ESP_OK) return nullptr;
    port_initialized = true;
    display = bsp_display_lcd_init(cfg);
    if (!display) { bsp_board_status.display = ESP_FAIL; goto failed; }
    {
        // Register display/touch while the port task is excluded.
        if (!lvgl_port_lock(0)) { bsp_board_status.display = ESP_FAIL; goto failed; }
        input = bsp_display_indev_init(display);
        if (!input) {
            if (bsp_board_status.touch == ESP_OK) bsp_board_status.touch = ESP_ERR_NO_MEM;
            ESP_LOGW(TAG, "Touch: %s", esp_err_to_name(bsp_board_status.touch));
            if (touch) { esp_lcd_touch_del(touch); touch = nullptr; }
            if (touch_io) { esp_lcd_panel_io_del(touch_io); touch_io = nullptr; }
        }
        lvgl_port_unlock();
    }
    bsp_board_status.display = esp_lcd_panel_disp_on_off(panel, true);
    if (bsp_board_status.display != ESP_OK) goto failed;
    // app_main releases the backlight only after the desktop has been drawn.
    display_started = true;
    ESP_LOGI(TAG, "ST7796 %dx%d RGB565; LVGL %d.%d.%d, esp_lvgl_port, PSRAM draw buffers / bounded DMA staging",
             BSP_LCD_H_RES, BSP_LCD_V_RES, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return display;
failed:
    ESP_LOGE(TAG, "Display startup failed: %s", esp_err_to_name(bsp_board_status.display));
    bsp_display_backlight_off();
    backlight_released = false;
    if (input) { lvgl_port_remove_touch(input); input = nullptr; }
    if (display) { lvgl_port_remove_disp(display); display = nullptr; }
    if (port_initialized) { lvgl_port_deinit(); port_initialized = false; }
    if (touch) { esp_lcd_touch_del(touch); touch = nullptr; }
    if (touch_io) { esp_lcd_panel_io_del(touch_io); touch_io = nullptr; }
    if (panel) { esp_lcd_panel_del(panel); panel = nullptr; }
    if (lcd_io) { esp_lcd_panel_io_del(lcd_io); lcd_io = nullptr; }
    heap_caps_free(lcd_dma_buffer); lcd_dma_buffer = nullptr;
    if (!sdcard && spi_ready) { spi_bus_free(BSP_LCD_SPI_HOST); spi_ready = false; }
    return nullptr;
}

lv_display_t *bsp_display_get(void) { return display; }
lv_indev_t *bsp_display_get_input_dev(void) { return input; }
esp_lcd_touch_handle_t bsp_touch_get_handle(void) { return touch; }
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_display_set_rotation(disp, rotation);
}
bool bsp_display_lock(uint32_t timeout_ms) { return lvgl_port_lock(timeout_ms); }
void bsp_display_unlock(void) { lvgl_port_unlock(); }
