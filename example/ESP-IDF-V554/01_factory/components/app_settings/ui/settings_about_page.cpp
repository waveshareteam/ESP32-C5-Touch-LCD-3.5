#include "settings_about_page.hpp"

#include "system_manage_service.h"
#include "settings_page_common.hpp"

namespace {

static lv_obj_t *add_about_row(lv_obj_t *list, const char *key, const char *value)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, 280, 48);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(row, 8, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *key_label = lv_label_create(row);
    lv_label_set_text(key_label, key);
    settings_ui::style_text(key_label, 0x697386);
    lv_obj_align(key_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *value_label = lv_label_create(row);
    lv_label_set_text(value_label, value);
    settings_ui::style_text(value_label, 0x141922);
    lv_obj_set_width(value_label, 158);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, 0, 0);
    return row;
}

} // namespace

lv_obj_t *settings_about_page_create(lv_obj_t *parent, lv_event_cb_t back_cb)
{
    lv_obj_t *page = lv_obj_create(parent);
    settings_ui::style_sheet(page);
    settings_ui::create_header(page, "About", back_cb, nullptr);

    lv_obj_t *list = lv_obj_create(page);
    lv_obj_set_size(list, 304, 330);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xF8F9FA), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    sys_manage_service_t *sms = get_system_manage_service_handle();
    add_about_row(list, "Device", sms && sms->device_name ? sms->device_name : "ESP32-C5");
    add_about_row(list, "Firmware", sms && sms->firmware_name ? sms->firmware_name : "Factory");
    add_about_row(list, "UI", "Brookesia Settings");
    return page;
}
