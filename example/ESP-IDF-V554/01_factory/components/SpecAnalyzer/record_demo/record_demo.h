#pragma once

#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RECORD_EVENT_START,
    RECORD_EVENT_STOP,
} record_event_t;

// 回调函数类型定义：第一个参数为事件类型，第二个为用户自定义数据
typedef void (*record_event_cb_t)(record_event_t event, void *user_data);

void audio_record_init(esp_codec_dev_handle_t dev);
void audio_record_deinit(void);
void audio_record_start(void);
void audio_record_stop(void);
bool audio_record_is_recording(void);

#ifdef __cplusplus
}
#endif
