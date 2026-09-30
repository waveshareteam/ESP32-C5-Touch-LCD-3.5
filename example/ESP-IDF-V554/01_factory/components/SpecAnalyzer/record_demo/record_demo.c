#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "record_demo.h"
#include "bsp_board_extra.h"
#include "esp_lv_adapter.h"

static const char *TAG = "AUDIO_RECORD";

#define BUFFER_SIZE     (1024)
#define SAMPLE_RATE     (16000)   // ← 改为 16000 Hz
#define CHANNELS        (1)       // ← 单通道
#define BITS_PER_SAMPLE (16)

// 标准 WAV 文件头结构 (44 字节)
typedef struct __attribute__((packed)) {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t format;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
} wav_header_t;

static SemaphoreHandle_t g_record_sem = NULL;
static volatile bool g_is_recording = false;
static volatile bool g_task_run = false;
static TaskHandle_t g_record_task_handle = NULL;
static esp_codec_dev_handle_t g_mic_codec_dev = NULL;

static record_event_cb_t g_record_cb = NULL;
static void *g_record_user_data = NULL;
static lv_obj_t *label_btn;
static lv_obj_t *record_btn;
static lv_obj_t *play_label;
static lv_obj_t *play_btn;
static lv_timer_t *play_state_timer;
static bool g_is_playing_recording;

static void dispatch_record_event(record_event_t event);
static void record_ui_init(void);
static void set_display_refresh_enabled(bool enabled);
static void stop_recording_playback(void);

static void audio_record_task(void *arg) {
    int16_t *recording_buffer = (int16_t *)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DEFAULT);
    if (!recording_buffer) {
        ESP_LOGE(TAG, "Failed to allocate recording buffer");
        vTaskDelete(NULL);
        return;
    }

    while (g_task_run) {
        if (xSemaphoreTake(g_record_sem, portMAX_DELAY) == pdTRUE && g_task_run) {

            FILE *record_file = fopen("/sdcard/recording.wav", "wb");
            if (!record_file) {
                ESP_LOGE(TAG, "Failed to open TF card file! errno: %d", errno);
                g_is_recording = false;
                continue;
            }

            // 1. 配置麦克风：16000 Hz，单通道，16 bit
            esp_codec_dev_sample_info_t fs = {
                .bits_per_sample = BITS_PER_SAMPLE,
                .channel         = CHANNELS,       // ← 单通道
                .sample_rate     = SAMPLE_RATE,    // ← 16000 Hz
            };
            esp_codec_dev_set_in_gain(g_mic_codec_dev, 42.0);
            if (esp_codec_dev_open(g_mic_codec_dev, &fs) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open microphone");
                fclose(record_file);
                g_is_recording = false;
                continue;
            }

            // 2. 写入占位文件头
            wav_header_t header = {0};
            fwrite(&header, sizeof(wav_header_t), 1, record_file);
            setvbuf(record_file, NULL, _IOFBF, BUFFER_SIZE);

            size_t total_data_bytes = 0;
            ESP_LOGI(TAG, "Recording started (16000Hz, mono)...");

            // 3. 循环采集
            while (g_is_recording) {
                if (esp_codec_dev_read(g_mic_codec_dev, recording_buffer, BUFFER_SIZE) == ESP_OK) {
                    size_t written = fwrite(recording_buffer, 1, BUFFER_SIZE, record_file);
                    total_data_bytes += written;
                    if (written != BUFFER_SIZE) {
                        ESP_LOGE(TAG, "TF card write failed or card is full");
                        g_is_recording = false;
                    }
                }
            }

            // 4. 更新 WAV 文件头（单通道参数）
            header = (wav_header_t){
                .riff          = {'R','I','F','F'},
                .fileSize      = 36 + total_data_bytes,
                .wave          = {'W','A','V','E'},
                .fmt           = {'f','m','t',' '},
                .fmtSize       = 16,
                .format        = 1,
                .channels      = CHANNELS,                                          // ← 1
                .sampleRate    = SAMPLE_RATE,                                       // ← 16000
                .byteRate      = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8),   // ← 32000
                .blockAlign    = CHANNELS * (BITS_PER_SAMPLE / 8),                 // ← 2
                .bitsPerSample = BITS_PER_SAMPLE,
                .data          = {'d','a','t','a'},
                .dataSize      = total_data_bytes
            };
            fseek(record_file, 0, SEEK_SET);
            fwrite(&header, sizeof(wav_header_t), 1, record_file);

            // 5. 关闭文件
            fclose(record_file);
            ESP_LOGI(TAG, "Recording stopped. Saved %u bytes to TF card.",
                     (unsigned)total_data_bytes);
            dispatch_record_event(RECORD_EVENT_STOP);
        }
    }

    if (g_record_sem) {
        xSemaphoreGive(g_record_sem);
        vSemaphoreDelete(g_record_sem);
        g_record_sem = NULL;
    }
    free(recording_buffer);
    g_record_task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_record_init(esp_codec_dev_handle_t dev)
{
    if (g_record_sem == NULL) {
        g_record_sem = xSemaphoreCreateBinary();
        g_mic_codec_dev = dev;
        g_task_run = true;
        if (xTaskCreate(audio_record_task, "audio_record_task", 4096, NULL, 5,
                        &g_record_task_handle) != pdPASS) {
            ESP_LOGE(TAG, "Failed to create recording task");
            vSemaphoreDelete(g_record_sem);
            g_record_sem = NULL;
            g_task_run = false;
            return;
        }
        ESP_LOGI(TAG, "Recording system initialized.");
    }
    record_ui_init();
}

void audio_record_start(void) {
    if (g_task_run && !g_is_recording) {
        g_is_recording = true;
        dispatch_record_event(RECORD_EVENT_START);
        xSemaphoreGive(g_record_sem);
    }
}

void audio_record_stop(void) {
    if (g_is_recording) {
        g_is_recording = false;
    }
}

void audio_record_deinit(void)
{
    g_task_run = false;
    g_is_recording = false;
    stop_recording_playback();
    if (play_state_timer != NULL) {
        lv_timer_delete(play_state_timer);
        play_state_timer = NULL;
    }
    if (g_record_sem != NULL) {
        xSemaphoreGive(g_record_sem);
    }
    for (int i = 0; i < 20 && g_record_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    g_mic_codec_dev = NULL;
    record_btn = NULL;
    label_btn = NULL;
    play_btn = NULL;
    play_label = NULL;
    ESP_LOGI(TAG, "Recording system deinitialized.");
}

bool audio_record_is_recording(void)
{
    return g_is_recording;
}

void audio_record_register_callback(record_event_cb_t cb, void *user_data)
{
    g_record_cb = cb;
    g_record_user_data = user_data;
}

static void dispatch_record_event(record_event_t event)
{
    if (g_record_cb) {
        g_record_cb(event, g_record_user_data);
    }
}

static void set_display_refresh_enabled(bool enabled)
{
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        return;
    }
    if (enabled) {
        esp_lv_adapter_set_dummy_draw(display, false);
        lv_obj_invalidate(lv_screen_active());
    } else {
        /*
         * Keep LVGL input/event processing alive, but prevent every subsequent
         * flush from touching the LCD on the SPI bus shared with the TF card.
         */
        esp_lv_adapter_set_dummy_draw(display, true);
    }
}

static void record_finished_async(void *user_data)
{
    (void)user_data;
    set_display_refresh_enabled(true);
    if (record_btn != NULL) {
        lv_obj_remove_state(record_btn, LV_STATE_CHECKED);
    }
    if (label_btn != NULL) {
        lv_label_set_text(label_btn, "REC start");
    }
}

static void stop_recording_playback(void)
{
    if (g_is_playing_recording) {
        Audio_Stop_Play();
        g_is_playing_recording = false;
    }
    set_display_refresh_enabled(true);
    if (play_btn != NULL) {
        lv_obj_remove_state(play_btn, LV_STATE_CHECKED);
    }
    if (play_label != NULL) {
        lv_label_set_text(play_label, "Play recording");
    }
}

static void play_state_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!g_is_playing_recording) {
        return;
    }

    esp_asp_state_t state = Audio_Get_Current_State();
    if (state == ESP_ASP_STATE_FINISHED || state == ESP_ASP_STATE_STOPPED ||
        state == ESP_ASP_STATE_ERROR) {
        stop_recording_playback();
    }
}

static void playback_event_handler(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (g_is_playing_recording) {
        stop_recording_playback();
        return;
    }
    if (audio_record_is_recording()) {
        return;
    }

    lv_obj_add_state(play_btn, LV_STATE_CHECKED);
    lv_label_set_text(play_label, "Playing - tap to stop");
    lv_refr_now(lv_display_get_default());
    set_display_refresh_enabled(false);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_gmf_err_t ret = Audio_Play_Music("file://sdcard/recording.wav");
    if (ret == ESP_GMF_ERR_OK) {
        g_is_playing_recording = true;
    } else {
        ESP_LOGE(TAG, "Failed to play recording: %d", ret);
        stop_recording_playback();
        lv_label_set_text(play_label, "Playback failed");
    }
}

static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        uint8_t is_on = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
        if (is_on == 1) {
            stop_recording_playback();
            lv_label_set_text(label_btn, "Recording");
            lv_refr_now(lv_display_get_default());
            set_display_refresh_enabled(false);
            /* Let the last queued LCD SPI transfer finish before TF writes. */
            vTaskDelay(pdMS_TO_TICKS(100));
            audio_record_start();
        } else {
            audio_record_stop();
            set_display_refresh_enabled(true);
            lv_label_set_text(label_btn, "REC start");
        }
    }
}

void my_record_handler(record_event_t event, void *user_data) {
    if (event == RECORD_EVENT_START) {
        printf("UI: Recording...\n");
    } else if (event == RECORD_EVENT_STOP) {
        printf("UI: Recording done.\n");
        lv_async_call(record_finished_async, NULL);
    }
}

static void record_ui_init(void)
{
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 320, 480);
    lv_obj_center(cont);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 8, 0);

    /* --- 音频参数展示 --- */
    lv_obj_t *label_sr = lv_label_create(cont);
    lv_label_set_text_fmt(label_sr, "Sample Rate: %d Hz", SAMPLE_RATE); // 16000

    lv_obj_t *label_ch = lv_label_create(cont);
    lv_label_set_text(label_ch, "Channels: 1 (Mono)"); // 单通道

    lv_obj_t *label_bit = lv_label_create(cont);
    lv_label_set_text(label_bit, "Bit Width: 16 bit");

    /* --- 文件路径展示 --- */
    lv_obj_t *label_path = lv_label_create(cont);
    lv_label_set_text(label_path, "Path: /sdcard/recording.wav");
    lv_obj_set_style_text_color(label_path, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(label_path, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(label_path, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_path, 200);

    /* --- 录音控制按钮 --- */
    record_btn = lv_button_create(cont);
    lv_obj_add_event_cb(record_btn, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(record_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(record_btn, 140, LV_SIZE_CONTENT);

    label_btn = lv_label_create(record_btn);
    lv_label_set_text(label_btn, "REC start");
    lv_obj_center(label_btn);

    play_btn = lv_button_create(cont);
    lv_obj_add_event_cb(play_btn, playback_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(play_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(play_btn, 180, LV_SIZE_CONTENT);

    play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "Play recording");
    lv_obj_center(play_label);

    g_is_playing_recording = false;
    if (play_state_timer == NULL) {
        play_state_timer = lv_timer_create(play_state_timer_cb, 200, NULL);
    }

    audio_record_register_callback(my_record_handler, NULL);
}
