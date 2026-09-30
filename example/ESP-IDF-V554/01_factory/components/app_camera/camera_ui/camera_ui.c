#include "camera_ui.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "example_video_common.h"
#include <linux/videodev2.h>

#define CAM_WIDTH    240
#define CAM_HEIGHT   320
#define BUFFER_COUNT 2
#define CAM_FORMAT   V4L2_PIX_FMT_RGB565
#define CAM_FRAME_SIZE (CAM_WIDTH * CAM_HEIGHT * 2)
#define CAM_FLIP_APPLY_DELAY_MS 500
#define CAM_CAPTURE_BUTTON_GPIO GPIO_NUM_28
#define CAM_CAPTURE_DEBOUNCE_MS 50
#define CAM_JPEG_QUALITY 80
#define CAM_PHOTO_DIR "/sdcard/photo"
/*
 * BF3901 format tables program register 0x1e to 0x39, which already enables
 * both mirror bits. The board's corrected preview orientation matches the
 * manual switch OFF/OFF state, so use that as the app-launch default.
 */
#define CAM_DEFAULT_VFLIP false
#define CAM_DEFAULT_HFLIP false
/* 定义通知值 */
#define CAM_NOTIFY_START  (1 << 0)
#define CAM_NOTIFY_STOP   (1 << 1)

/**
 * @brief 摄像头信息结构体
 */
typedef struct {
    /* VIDIOC_QUERYCAP */
    char     driver[16];        // 驱动名称
    char     card[32];          // 设备/卡名称
    uint32_t version;           // 驱动版本号
    uint32_t capabilities;      // 设备能力标志

    /* VIDIOC_G_FMT */
    uint32_t width;             // 当前宽度（像素）
    uint32_t height;            // 当前高度（像素）
    uint32_t pixelformat;       // 当前像素格式（如 V4L2_PIX_FMT_RGB565）
    char     pixelformat_str[5];// 像素格式四字符码（可读）
    uint32_t bytesperline;      // 每行字节数
    uint32_t sizeimage;         // 单帧图像字节数

    /* VIDIOC_G_EXT_CTRLS */
    int32_t  vflip;             // 垂直翻转状态
    int32_t  hflip;             // 水平镜像状态
    int32_t  exposure;          // 曝光值（-1 表示当前传感器不支持读取）
    int32_t  gain;              // 增益值（-1 表示当前传感器不支持读取）
} cam_info_t;

static const char *TAG = "camera ui";
static lv_display_t *disp_handle;
static TaskHandle_t s_cam_task_handle = NULL;
static volatile bool s_stream_running = false;
static volatile bool s_stream_active = false;
static example_encoder_handle_t s_jpeg_encoder = NULL;
static uint8_t *s_jpeg_buffer = NULL;
static uint32_t s_jpeg_buffer_size = 0;

/* 镜像状态（可在任意上下文中修改，任务循环读取） */
static volatile bool s_vflip = CAM_DEFAULT_VFLIP;
static volatile bool s_hflip = CAM_DEFAULT_HFLIP;

/* 摄像头设备文件描述符（供外部控制函数使用） */
static int s_cam_fd = -1;

static lv_obj_t *cam_play_bg = NULL;
static lv_obj_t *s_capture_label = NULL;
static lv_obj_t *s_photo_screen = NULL;
static cam_info_t cam_info;

static void camera_stream_task(void *arg);
static void camera_apply_flip_task(void *arg);
static void cam_apply_flip_controls(const char *reason);
static void cam_set_vflip(bool enable);
static void cam_set_hflip(bool enable);
static esp_err_t cam_set_control(uint32_t id, int32_t value);
static int cam_read_info(int fd, cam_info_t *info);
static void cam_info_ui_show(lv_obj_t *parent, const cam_info_t *info);
static void create_flip_control_ui(lv_obj_t *parent);
static esp_err_t camera_capture_init(void);
static esp_err_t camera_save_jpeg(const uint8_t *frame, uint32_t frame_size);
static void camera_rgb565_to_yuyv(uint8_t *frame, uint32_t pixel_count);
static void camera_capture_status(const char *text, bool visible);

esp_err_t app_camera_drv_init(lv_display_t *disp)
{
    if(disp==NULL)
    {
        return ESP_FAIL;
    }
    disp_handle = disp;

    example_video_init();

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << CAM_CAPTURE_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&button_config));

    esp_err_t capture_ret = camera_capture_init();
    if (capture_ret != ESP_OK) {
        ESP_LOGE(TAG, "photo encoder init failed: %s", esp_err_to_name(capture_ret));
    }

    xTaskCreate(
        camera_stream_task,
        "cam_stream",
        8192,        
        NULL,
        5,          
        &s_cam_task_handle
    );
    return ESP_OK;
}

static esp_err_t camera_capture_init(void)
{
    example_encoder_config_t config = {
        .width = CAM_WIDTH,
        .height = CAM_HEIGHT,
        /* esp_new_jpeg on ESP32-C5 does not accept RGB565 input. The captured
         * frame is converted to YUYV in-place immediately before encoding. */
        .pixel_format = V4L2_PIX_FMT_YUYV,
        .quality = CAM_JPEG_QUALITY,
    };

    esp_err_t ret = example_encoder_init(&config, &s_jpeg_encoder);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = example_encoder_alloc_output_buffer(s_jpeg_encoder, &s_jpeg_buffer,
                                               &s_jpeg_buffer_size);
    if (ret != ESP_OK) {
        example_encoder_deinit(s_jpeg_encoder);
        s_jpeg_encoder = NULL;
    }
    return ret;
}

static uint8_t camera_clamp_u8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static void camera_rgb565_to_yuyv(uint8_t *frame, uint32_t pixel_count)
{
    uint16_t *rgb = (uint16_t *)frame;

    /* Read both RGB565 pixels before overwriting the same four bytes with
     * Y0/U/Y1/V, allowing conversion without another full-frame buffer. */
    for (uint32_t i = 0; i + 1 < pixel_count; i += 2) {
        uint16_t p0 = rgb[i];
        uint16_t p1 = rgb[i + 1];
        int r0 = ((p0 >> 11) & 0x1f) * 255 / 31;
        int g0 = ((p0 >> 5) & 0x3f) * 255 / 63;
        int b0 = (p0 & 0x1f) * 255 / 31;
        int r1 = ((p1 >> 11) & 0x1f) * 255 / 31;
        int g1 = ((p1 >> 5) & 0x3f) * 255 / 63;
        int b1 = (p1 & 0x1f) * 255 / 31;
        int y0 = (77 * r0 + 150 * g0 + 29 * b0) >> 8;
        int y1 = (77 * r1 + 150 * g1 + 29 * b1) >> 8;
        int u = ((-43 * (r0 + r1) - 85 * (g0 + g1) + 128 * (b0 + b1)) >> 9) + 128;
        int v = ((128 * (r0 + r1) - 107 * (g0 + g1) - 21 * (b0 + b1)) >> 9) + 128;
        uint8_t *out = frame + i * 2;
        out[0] = camera_clamp_u8(y0);
        out[1] = camera_clamp_u8(u);
        out[2] = camera_clamp_u8(y1);
        out[3] = camera_clamp_u8(v);
    }
}

static void camera_capture_status(const char *text, bool visible)
{
    if (s_capture_label == NULL || bsp_display_lock(1000) != ESP_OK) {
        return;
    }

    if (text != NULL) {
        lv_label_set_text(s_capture_label, text);
    }
    if (visible) {
        lv_obj_remove_flag(s_capture_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_capture_label, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_refresh_now(disp_handle);
    bsp_display_unlock();
}

static esp_err_t camera_save_jpeg(const uint8_t *frame, uint32_t frame_size)
{
    if (s_jpeg_encoder == NULL || s_jpeg_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t jpeg_size = 0;
    esp_err_t ret = example_encoder_process(s_jpeg_encoder, (uint8_t *)frame, frame_size,
                                            s_jpeg_buffer, s_jpeg_buffer_size, &jpeg_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG encode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mkdir(CAM_PHOTO_DIR, 0775) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "create %s failed: errno=%d", CAM_PHOTO_DIR, errno);
        return ESP_FAIL;
    }

    char path[96];
    uint32_t id = (uint32_t)(esp_timer_get_time() / 1000ULL);
    for (uint32_t suffix = 0; suffix < 1000; suffix++) {
        snprintf(path, sizeof(path), CAM_PHOTO_DIR "/photo_%010" PRIu32 "_%03" PRIu32 ".jpg",
                 id, suffix);
        if (access(path, F_OK) != 0) {
            break;
        }
        if (suffix == 999) {
            ESP_LOGE(TAG, "cannot allocate a unique photo name");
            return ESP_ERR_NOT_FOUND;
        }
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        ESP_LOGE(TAG, "open %s failed: errno=%d", path, errno);
        return ESP_FAIL;
    }
    size_t written = fwrite(s_jpeg_buffer, 1, jpeg_size, file);
    int close_ret = fclose(file);
    if (written != jpeg_size || close_ret != 0) {
        ESP_LOGE(TAG, "write %s failed (%u/%u bytes)", path,
                 (unsigned int)written, (unsigned int)jpeg_size);
        unlink(path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "photo saved: %s (%u bytes)", path, (unsigned int)jpeg_size);
    return ESP_OK;
}


static void stop_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {

        cam_stream_stop();
    }
}

esp_err_t app_camera_ui_start(void)
{
    // ===================== 创建主屏幕容器 =====================
    lv_obj_t *photo_screen = lv_obj_create(lv_screen_active());
    s_photo_screen = photo_screen;
    lv_obj_set_size(photo_screen, 320, 480);
    lv_obj_align(photo_screen, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(photo_screen, lv_color_hex(0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(photo_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(photo_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(photo_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(photo_screen, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(photo_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(photo_screen, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(photo_screen, 4, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(photo_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(photo_screen, LV_OBJ_FLAG_SCROLLABLE);

    //cam_set_vflip(0);

    if (cam_read_info(s_cam_fd, &cam_info) == 0) {
        ESP_LOGI(TAG, "Camera ready: %ux%u [%s]",
                cam_info.width, cam_info.height, cam_info.pixelformat_str);
    }
    cam_info_ui_show(photo_screen,&cam_info);
    create_flip_control_ui(photo_screen);

    cam_play_bg = lv_button_create(lv_screen_active());
    lv_obj_set_size(cam_play_bg, 320, 480);
    lv_obj_set_style_bg_color(cam_play_bg, lv_color_hex(0x000000), LV_STATE_DEFAULT); 
    lv_obj_add_event_cb(cam_play_bg, stop_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(cam_play_bg, LV_ALIGN_CENTER, 0, 0);

    s_capture_label = lv_label_create(cam_play_bg);
    lv_label_set_text(s_capture_label, "Saving photo...");
    lv_obj_set_style_text_color(s_capture_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_capture_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_capture_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_capture_label, LV_OBJ_FLAG_HIDDEN);
    //lv_obj_add_flag(cam_play_bg, LV_OBJ_FLAG_HIDDEN);

    esp_lv_adapter_refresh_now(disp_handle);
    return ESP_OK;
}

void app_camera_ui_destroy(void)
{
    if (cam_play_bg != NULL) {
        lv_obj_delete(cam_play_bg);
        cam_play_bg = NULL;
        s_capture_label = NULL;
    }
    if (s_photo_screen != NULL) {
        lv_obj_delete(s_photo_screen);
        s_photo_screen = NULL;
    }
}





void cam_stream_start(void)
{
    if (s_cam_task_handle == NULL) {
        ESP_LOGE(TAG, "camera task not running");
        return;
    }
    if (cam_play_bg != NULL) {
        lv_obj_remove_flag(cam_play_bg, LV_OBJ_FLAG_HIDDEN);
        esp_lv_adapter_refresh_now(disp_handle);
    }
    if (s_stream_running || s_stream_active) {
        ESP_LOGI(TAG, "stream already running");
        return;
    }
    s_stream_running = true;   // 先设置标志
    xTaskNotify(s_cam_task_handle, CAM_NOTIFY_START, eSetBits);
    xTaskCreate(camera_apply_flip_task, "cam_flip", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "stream start requested");
}

void cam_stream_stop(void)
{
    if (s_cam_task_handle == NULL) {
        ESP_LOGE(TAG, "camera task not running");
        return;
    }
    if (cam_play_bg != NULL) {
        lv_obj_add_flag(cam_play_bg, LV_OBJ_FLAG_HIDDEN);
        esp_lv_adapter_refresh_now(disp_handle);
    }
    if (!s_stream_running) {
        ESP_LOGI(TAG, "stream already stopped");
        return;
    }
    s_stream_running = false;  // 先清除标志，任务循环会检测到
    // 可选：发通知唤醒任务（如果 DQBUF 会阻塞则需要）
    xTaskNotify(s_cam_task_handle, CAM_NOTIFY_STOP, eSetBits);
    ESP_LOGI(TAG, "stream stop requested");
}



/**
 * @brief 设置垂直翻转（V4L2_CID_VFLIP）
 */
static esp_err_t cam_set_control(uint32_t id, int32_t value)
{
    if (s_cam_fd < 0) {
        ESP_LOGW(TAG, "cam_set_control: device not open (fd=%d)", s_cam_fd);
        return ESP_ERR_INVALID_STATE;
    }

    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_USER,
        .count = 1,
    };
    struct v4l2_ext_control control = {
        .id = id,
        .value = value,
    };
    controls.controls = &control;

    return ioctl(s_cam_fd, VIDIOC_S_EXT_CTRLS, &controls) == 0 ? ESP_OK : ESP_FAIL;
}

static void cam_set_vflip(bool enable)
{
    if (cam_set_control(V4L2_CID_VFLIP, enable ? 1 : 0) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set vflip");
    } else {
        s_vflip = enable;
        ESP_LOGI(TAG, "vflip: %s", enable ? "ON" : "OFF");
    }
}

/**
 * @brief 设置水平镜像（V4L2_CID_HFLIP）
 */
static void cam_set_hflip(bool enable)
{
    if (cam_set_control(V4L2_CID_HFLIP, enable ? 1 : 0) != ESP_OK) {
        ESP_LOGW(TAG, "failed to set hflip");
    } else {
        s_hflip = enable;
        ESP_LOGI(TAG, "hflip: %s", enable ? "ON" : "OFF");
    }
}

static void cam_apply_flip_controls(const char *reason)
{
    ESP_LOGI(TAG, "%s: apply flip v=%d, h=%d",
             reason, s_vflip ? 1 : 0, s_hflip ? 1 : 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT(cam_set_control(V4L2_CID_VFLIP, s_vflip ? 1 : 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(cam_set_control(V4L2_CID_HFLIP, s_hflip ? 1 : 0));
}

static void camera_apply_flip_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(CAM_FLIP_APPLY_DELAY_MS));

    if (s_stream_running && s_cam_fd >= 0) {
        cam_apply_flip_controls("after stream settle");
    }

    vTaskDelete(NULL);
}


static void camera_stream_task(void *arg)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint8_t  *buffer[BUFFER_COUNT];
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    s_cam_fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    if (s_cam_fd < 0) {
        ESP_LOGE(TAG, "failed to open device");
        vTaskDelete(NULL);
        return;
    }

    struct v4l2_format format = {
        .type = type,
        .fmt.pix.width       = CAM_WIDTH,
        .fmt.pix.height      = CAM_HEIGHT,
        .fmt.pix.pixelformat = CAM_FORMAT,
    };
    if (ioctl(s_cam_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to set format");
        goto cleanup;
    }

    memset(&req, 0, sizeof(req));
    req.count  = BUFFER_COUNT;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_cam_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        goto cleanup;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        ioctl(s_cam_fd, VIDIOC_QUERYBUF, &buf);
        buffer[i] = (uint8_t *)mmap(NULL, buf.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED, s_cam_fd, buf.m.offset);
        if (buffer[i] == MAP_FAILED) {
            ESP_LOGE(TAG, "mmap buffer %d failed", i);
            goto cleanup;
        }
    }

    /* 主循环：只用 xTaskNotifyWait 等待 START，不在推流循环内调用它 */
    while (1) {
        uint32_t notify_val = 0;

        /* 阻塞等待，退出时清除所有位 */
        xTaskNotifyWait(0, ULONG_MAX, &notify_val, portMAX_DELAY);

        if (!(notify_val & CAM_NOTIFY_START)) {
            /* 收到的不是 START（比如多余的 STOP），忽略 */
            ESP_LOGI(TAG, "ignoring non-start notification: 0x%lx", notify_val);
            continue;
        }

        /* STREAMON 前应用方向校正，避免默认打开相机时首帧方向不对。 */
        cam_apply_flip_controls("before stream on");

        /* 入队所有缓冲区 */
        for (int i = 0; i < BUFFER_COUNT; i++) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index  = i;
            ioctl(s_cam_fd, VIDIOC_QBUF, &buf);
        }

        if (ioctl(s_cam_fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
            s_stream_running = false;
            continue;
        }
        s_stream_active = true;

        /*
         * SPI camera may reinitialize the sensor during STREAMON, which can reset
         * mirror registers. Apply orientation again here so default app launch
         * matches the manual switch behavior.
         */
        cam_apply_flip_controls("after stream on");

        esp_lv_adapter_set_dummy_draw(disp_handle, true);
        ESP_LOGI(TAG, "stream started");
        bool frame_logged = false;
        bool stream_flip_applied = false;
        bool button_was_pressed = false;
        bool button_candidate = false;
        int64_t button_change_time_us = esp_timer_get_time();

        /* 推流循环：只检查 volatile 标志，不调用 xTaskNotifyWait */
        while (s_stream_running) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(s_cam_fd, VIDIOC_DQBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to dequeue frame");
                s_stream_running = false;
                break;
            }

            if (buf.flags & V4L2_BUF_FLAG_DONE) {
                if (!frame_logged) {
                    ESP_LOGI(TAG, "first frame: index=%u, bytesused=%u, length=%u",
                             (unsigned int)buf.index,
                             (unsigned int)buf.bytesused,
                             (unsigned int)buf.length);
                    frame_logged = true;
                }
                if (buf.bytesused < CAM_FRAME_SIZE) {
                    ESP_LOGW(TAG, "skip short frame: bytesused=%u, expected=%u",
                             (unsigned int)buf.bytesused,
                             (unsigned int)CAM_FRAME_SIZE);
                    ioctl(s_cam_fd, VIDIOC_QBUF, &buf);
                    continue;
                }
                if (!stream_flip_applied) {
                    cam_apply_flip_controls("after first frame");
                    stream_flip_applied = true;
                    ioctl(s_cam_fd, VIDIOC_QBUF, &buf);
                    continue;
                }

                bool button_pressed = gpio_get_level(CAM_CAPTURE_BUTTON_GPIO) == 0;
                int64_t now_us = esp_timer_get_time();
                if (button_pressed != button_candidate) {
                    button_candidate = button_pressed;
                    button_change_time_us = now_us;
                } else if (button_candidate != button_was_pressed &&
                           now_us - button_change_time_us >= CAM_CAPTURE_DEBOUNCE_MS * 1000LL) {
                    button_was_pressed = button_candidate;
                    if (button_was_pressed) {
                        esp_lv_adapter_set_dummy_draw(disp_handle, false);
                        camera_capture_status("Saving photo...", true);
                        camera_rgb565_to_yuyv(buffer[buf.index], CAM_WIDTH * CAM_HEIGHT);
                        esp_err_t capture_ret = camera_save_jpeg(buffer[buf.index], CAM_FRAME_SIZE);
                        if (capture_ret != ESP_OK) {
                            ESP_LOGE(TAG, "capture failed: %s", esp_err_to_name(capture_ret));
                            camera_capture_status("Save failed", true);
                        } else {
                            camera_capture_status("Photo saved", true);
                        }
                        vTaskDelay(pdMS_TO_TICKS(capture_ret == ESP_OK ? 300 : 800));
                        camera_capture_status(NULL, false);
                        esp_lv_adapter_set_dummy_draw(disp_handle, true);
                        ioctl(s_cam_fd, VIDIOC_QBUF, &buf);
                        continue;
                    }
                }

                uint16_t *pixels = (uint16_t *)buffer[buf.index];
                uint32_t  pixel_count = CAM_WIDTH * CAM_HEIGHT;
                for (uint32_t i = 0; i < pixel_count; i++) {
                    uint16_t p = pixels[i];
                    pixels[i] = (p >> 8) | (p << 8);
                }
                esp_err_t ret = esp_lv_adapter_dummy_draw_blit(
                    disp_handle, 40, 80, 280, 400,
                    buffer[buf.index], false
                );
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "dummy draw blit failed: %s", esp_err_to_name(ret));
                }
            }

            ioctl(s_cam_fd, VIDIOC_QBUF, &buf);
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        /* 停止流 */
        ioctl(s_cam_fd, VIDIOC_STREAMOFF, &type);
        s_stream_active = false;
        esp_lv_adapter_set_dummy_draw(disp_handle, false);
        /* 清除任务通知中可能残留的 STOP 位，避免下次循环误判 */
        xTaskNotifyWait(0, ULONG_MAX, NULL, 0);
        ESP_LOGI(TAG, "stream stopped");
    }

cleanup:
    close(s_cam_fd);
    s_cam_fd = -1;
    vTaskDelete(NULL);
}

/**
 * @brief 读取摄像头信息
 * @param fd    已打开的摄像头文件描述符
 * @param info  输出的信息结构体指针
 * @return 0 成功，-1 失败
 */
static int cam_read_info(int fd, cam_info_t *info)
{
    if (fd < 0 || info == NULL) return -1;
    memset(info, 0, sizeof(cam_info_t));

    /* 1. 查询设备能力 */
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        strncpy(info->driver, (char *)cap.driver, sizeof(info->driver) - 1);
        strncpy(info->card,   (char *)cap.card,   sizeof(info->card)   - 1);
        info->version      = cap.version;
        info->capabilities = cap.capabilities;
        ESP_LOGI(TAG, "driver: %s, card: %s, version: %u",
                 info->driver, info->card, info->version);
    } else {
        ESP_LOGW(TAG, "VIDIOC_QUERYCAP failed");
    }

    /* 2. 获取当前格式 */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
        info->width        = fmt.fmt.pix.width;
        info->height       = fmt.fmt.pix.height;
        info->pixelformat  = fmt.fmt.pix.pixelformat;
        info->bytesperline = fmt.fmt.pix.bytesperline;
        info->sizeimage    = fmt.fmt.pix.sizeimage;
        /* 将四字符码转为可读字符串 */
        info->pixelformat_str[0] = (char)((fmt.fmt.pix.pixelformat >>  0) & 0xFF);
        info->pixelformat_str[1] = (char)((fmt.fmt.pix.pixelformat >>  8) & 0xFF);
        info->pixelformat_str[2] = (char)((fmt.fmt.pix.pixelformat >> 16) & 0xFF);
        info->pixelformat_str[3] = (char)((fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        info->pixelformat_str[4] = '\0';
        ESP_LOGI(TAG, "format: %ux%u, pixfmt: %s, sizeimage: %u",
                 info->width, info->height,
                 info->pixelformat_str, info->sizeimage);
    } else {
        ESP_LOGW(TAG, "VIDIOC_G_FMT failed");
    }

    /*
     * BF3901 目前只实现了 vflip/hmirror 的 set/query，get_para_value()
     * 未实现。使用 VIDIOC_G_EXT_CTRLS 会触发底层红色错误日志；这里显示
     * APP 自己维护的翻转状态，避免把“不支持读取”当成运行错误。
     */
    info->vflip = s_vflip ? 1 : 0;
    info->hflip = s_hflip ? 1 : 0;
    info->exposure = -1;
    info->gain = -1;
    ESP_LOGI(TAG, "vflip=%d, hflip=%d, exposure/gain read unsupported",
             info->vflip, info->hflip);

    return 0;
}

static void cam_info_ui_show(lv_obj_t *parent, const cam_info_t *info)
{
    if (parent == NULL || info == NULL) return;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 300, 320);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x4A90D9), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 8, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Camera Info");
    lv_obj_set_style_text_color(title, lv_color_hex(0x4A90D9), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);

    /* 分隔线 */
    lv_obj_t *line = lv_obj_create(panel);
    lv_obj_set_size(line, 276, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x4A90D9), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(line, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);

    #define ADD_INFO_ROW(parent, key, fmt, ...) \
    do { \
        lv_obj_t *_lbl = lv_label_create(parent); \
        lv_label_set_text_fmt(_lbl, key ": " fmt, ##__VA_ARGS__); \
        lv_obj_set_style_text_color(_lbl, lv_color_hex(0xE0E0E0), LV_PART_MAIN); \
        lv_obj_set_style_text_font(_lbl, &lv_font_montserrat_14, LV_PART_MAIN); \
        lv_label_set_long_mode(_lbl, LV_LABEL_LONG_CLIP); \
        lv_obj_set_width(_lbl, 276); \
    } while(0)

    /* 设备能力：driver/card 是 char[]，直接用 %s，无需 PRI 宏 */
    ADD_INFO_ROW(panel, "Driver",  "%s", info->driver);
    ADD_INFO_ROW(panel, "Card",    "%s", info->card);

    /* version 是 uint32_t，拆分后每段强转为 unsigned int 使用 %u，
       或直接用 PRIu32 */
    ADD_INFO_ROW(panel, "Version", "%" PRIu32 ".%" PRIu32 ".%" PRIu32,
                 (uint32_t)((info->version >> 16) & 0xFF),
                 (uint32_t)((info->version >>  8) & 0xFF),
                 (uint32_t)( info->version        & 0xFF));

    /* 格式信息：width/height/bytesperline/sizeimage 均为 uint32_t */
    ADD_INFO_ROW(panel, "Resolution",   "%" PRIu32 " x %" PRIu32,
                 info->width, info->height);
    ADD_INFO_ROW(panel, "PixelFmt",     "%s",        info->pixelformat_str);
    ADD_INFO_ROW(panel, "BytesPerLine", "%" PRIu32,  info->bytesperline);
    ADD_INFO_ROW(panel, "FrameSize",    "%" PRIu32 " B", info->sizeimage);

    /* 控制参数：vflip/hflip 是 int32_t 但只显示字符串，exposure/gain 是 int32_t */
    ADD_INFO_ROW(panel, "VFlip",    "%s",        info->vflip ? "ON" : "OFF");
    ADD_INFO_ROW(panel, "HFlip",    "%s",        info->hflip ? "ON" : "OFF");
    if (info->exposure >= 0) {
        ADD_INFO_ROW(panel, "Exposure", "%" PRId32, info->exposure);
    } else {
        ADD_INFO_ROW(panel, "Exposure", "%s", "N/A");
    }
    if (info->gain >= 0) {
        ADD_INFO_ROW(panel, "Gain", "%" PRId32, info->gain);
    } else {
        ADD_INFO_ROW(panel, "Gain", "%s", "N/A");
    }

    #undef ADD_INFO_ROW
}



static void sw_vflip_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    cam_set_vflip(is_checked);
    printf("cam_set_vflip(%d)\r\n",is_checked);
}


static void sw_hflip_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    printf("cam_set_hflip(%d)\r\n",is_checked);
    cam_set_hflip(is_checked);
}


// Start 按钮事件回调
static void start_btn_event_cb(lv_event_t *e)
{
    cam_stream_start();
}

/**
 * 创建 320*100 容器，包含两个开关 + Start 按钮（带事件）
 */
void create_flip_control_ui(lv_obj_t *parent)
{
    // 主容器：320×80（完美匹配父对象大小）
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 300, 100);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 6, 0); // 更小内边距

    // ==================== VFLIP 开关组（缩小版） ====================
    lv_obj_t *v_cont = lv_obj_create(cont);
    lv_obj_set_size(v_cont, 90, 80);
    lv_obj_set_flex_flow(v_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(v_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(v_cont, 0, 0);

    lv_obj_t *label_v = lv_label_create(v_cont);
    lv_label_set_text(label_v, "VFLIP");
    lv_obj_t *s_sw_vflip = lv_switch_create(v_cont);
    lv_obj_set_size(s_sw_vflip, 40, 25); // 开关缩小
    if (s_vflip) {
        lv_obj_add_state(s_sw_vflip, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_sw_vflip, sw_vflip_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ==================== HFLIP 开关组（缩小版） ====================
    lv_obj_t *h_cont = lv_obj_create(cont);
    lv_obj_set_size(h_cont, 90, 60);
    lv_obj_set_flex_flow(h_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(h_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(h_cont, 0, 0);

    lv_obj_t *label_h = lv_label_create(h_cont);
    lv_label_set_text(label_h, "HFLIP");
    lv_obj_t *s_sw_hflip = lv_switch_create(h_cont);
    lv_obj_set_size(s_sw_hflip, 40, 20); // 开关缩小
    if (s_hflip) {
        lv_obj_add_state(s_sw_hflip, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_sw_hflip, sw_hflip_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ==================== START 按钮（缩小版） ====================
    lv_obj_t *start_btn = lv_btn_create(cont);
    lv_obj_set_size(start_btn, 90, 60);
    lv_obj_add_event_cb(start_btn, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(start_btn);
    lv_label_set_text(btn_label, "start");
    lv_obj_center(btn_label);
}
