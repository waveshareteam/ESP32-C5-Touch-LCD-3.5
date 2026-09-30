#pragma once

#include "lvgl.h"

namespace settings_ui {

void style_text(lv_obj_t *obj, uint32_t color);
void style_sheet(lv_obj_t *obj);
void style_list(lv_obj_t *list);
void style_list_header(lv_obj_t *label);
void style_list_row(lv_obj_t *row);
lv_obj_t *create_header(lv_obj_t *page, const char *title, lv_event_cb_t back_cb, lv_event_cb_t action_cb);
lv_obj_t *create_primary_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb);
lv_obj_t *create_detail_panel(lv_obj_t *page);

} // namespace settings_ui
