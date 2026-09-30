#pragma once

#include "lvgl.h"

lv_obj_t *settings_power_page_create(lv_obj_t *parent, lv_event_cb_t back_cb);
void settings_power_page_refresh(void);
void settings_power_page_start(void);
void settings_power_page_stop(void);
