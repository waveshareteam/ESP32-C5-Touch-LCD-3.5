#pragma once

#include "esp_log.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_camera_drv_init(lv_display_t *disp);
esp_err_t app_camera_ui_start(void);
void app_camera_ui_destroy(void);
void cam_stream_start(void);
void cam_stream_stop(void);

#ifdef __cplusplus
}
#endif
