#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void music_player_init(void);
void music_ui_create(lv_obj_t *parent);
void music_ui_destroy(void);
void music_ui_stop(void);
void music_ui_pause(void);
void music_ui_resume(void);

#ifdef __cplusplus
}
#endif
