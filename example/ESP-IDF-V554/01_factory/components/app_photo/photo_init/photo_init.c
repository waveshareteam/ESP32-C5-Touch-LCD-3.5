#include "photo_init.h"

#include "esp_jpeg_dec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

static const char *TAG = "photo";


static generic_file_list_t *jpg_file_list = NULL;
static lv_display_t      *s_disp;
static int32_t current_photo_idx = 0;


static lv_image_dsc_t img_dsc;
static jpeg_dec_handle_t dec_handle = NULL;
static uint16_t *jpeg_out_framebuf = NULL;
static lv_obj_t * photo_img_obj = NULL;

#define VIDEO_W   320
#define VIDEO_H   480

// 从 SD 卡读取文件到内存缓冲区
uint8_t* read_file_to_buffer(const char* path, size_t* size) 
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // ESP32-S3 要求输入缓冲区 16 字节对齐
    size_t aligned_size = (*size + 15) & ~15;
    uint8_t* buf = (uint8_t*)jpeg_calloc_align(aligned_size, 16); // 明确对齐到 16 字节
    
    if (buf) {
        fread(buf, 1, *size, f);
    }
    
    fclose(f);
    return buf;
}


void update_photo_display(int32_t path_index)
{
    size_t jpeg_size = 0;
    printf("%s\r\n",jpg_file_list->list[path_index]);
    uint8_t* jpeg_buf = read_file_to_buffer(jpg_file_list->list[path_index], &jpeg_size); 
    if (!jpeg_buf) {
        ESP_LOGE(TAG, "Failed to read file to buffer");
        return;
    }

    // 初始化 IO 结构体
    jpeg_dec_io_t io = {
        .inbuf = jpeg_buf,
        .inbuf_len = (int)jpeg_size,
        .outbuf = (uint8_t*)jpeg_out_framebuf
    };

    // 1. 解析头信息
    jpeg_dec_header_info_t header_info;
    if (jpeg_dec_parse_header(dec_handle, &io, &header_info) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_parse_header failed");

    }

    int outbuf_len = 0;
    jpeg_dec_get_outbuf_len(dec_handle, &outbuf_len);
    

    // 2. 执行解码
    if (jpeg_dec_process(dec_handle, &io) == JPEG_ERR_OK) {
        // 5. LVGL 显示逻辑

        img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        img_dsc.header.w = header_info.width;
        img_dsc.header.h = header_info.height;
        img_dsc.data_size = outbuf_len;
        img_dsc.data = (uint8_t*)jpeg_out_framebuf;

        lv_image_set_src(photo_img_obj, &img_dsc);
    } 
    else 
    {
        ESP_LOGE(TAG, "jpeg_dec_process failed");

    }

    jpeg_free_align(jpeg_buf);
}



static void photo_play_event_handler(lv_event_t * e) 
{
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) 
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        
        if (dir == LV_DIR_LEFT ) 
        {
            current_photo_idx++;
            if(current_photo_idx > jpg_file_list->count-1)
            {
                current_photo_idx  = 0;
            }
            update_photo_display(current_photo_idx);
        } 
        else if (dir == LV_DIR_RIGHT) 
        {
            current_photo_idx--;
            if(current_photo_idx < 0)
            {
                current_photo_idx  = jpg_file_list->count-1;
            }
            update_photo_display(current_photo_idx);
        } 
    }
}

void init_photo_ui_screen(lv_obj_t *parent,generic_file_list_t *file_list)
{
    
    if(file_list==NULL || parent==NULL)
    {
        return;   
    }
    jpg_file_list = file_list;

    // ===================== 屏幕背景样式 =====================
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), LV_PART_MAIN);  
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // ===================== 创建主屏幕容器 =====================
    lv_obj_t *photo_screen = lv_obj_create(parent);
    lv_obj_set_size(photo_screen, 320, 480);
    lv_obj_align(photo_screen, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(photo_screen, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(photo_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(photo_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(photo_screen, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(photo_screen, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(photo_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(photo_screen, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(photo_screen, 4, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(photo_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(photo_screen, LV_OBJ_FLAG_SCROLLABLE);

    if(jpg_file_list->count == 0)
    {
        lv_obj_t * label1 = lv_label_create(lv_screen_active());
        lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);     /*Break the long lines*/
        lv_label_set_text(label1, "can't find jpg photo!");
        lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);
        return;  
    }

        // image
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc.data = NULL;

    photo_img_obj = lv_image_create(photo_screen);
    lv_image_set_src(photo_img_obj, &img_dsc);
    lv_obj_center(photo_img_obj);
    lv_obj_remove_flag(photo_img_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(lv_screen_active(), photo_play_event_handler, LV_EVENT_GESTURE, NULL);

    current_photo_idx = 0;
    update_photo_display(current_photo_idx);
    esp_lv_adapter_refresh_now(s_disp);


}


void init_photo_player(lv_display_t *disp)
{
    s_disp = disp;

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE; 
    
    if (jpeg_dec_open(&config, &dec_handle) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_open failed");
        return;
    }

    size_t buf_size = VIDEO_W * VIDEO_H * sizeof(uint16_t);
    jpeg_out_framebuf = (uint16_t *)jpeg_calloc_align(buf_size, 16);
}

/**
 * @brief 释放照片播放器资源
 */
void deinit_photo_player(void)
{
    // 1. 释放解码器硬件资源
    if (dec_handle) {
        jpeg_dec_close(dec_handle);
        dec_handle = NULL;
    }

    // 2. 释放对齐分配的输出帧缓存
    if (jpeg_out_framebuf) {
        jpeg_free_align(jpeg_out_framebuf);
        jpeg_out_framebuf = NULL;
    }

    ESP_LOGI(TAG, "Photo player deinitialized");
}