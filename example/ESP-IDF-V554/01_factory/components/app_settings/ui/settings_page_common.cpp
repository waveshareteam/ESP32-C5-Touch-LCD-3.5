#include "settings_page_common.hpp"

namespace settings_ui {

void style_text(lv_obj_t *obj, uint32_t color)
{
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(obj, 0, LV_PART_MAIN);
}

void style_sheet(lv_obj_t *obj)
{
    lv_obj_set_size(obj, 320, 432);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xF8F9FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 8, LV_PART_MAIN);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void style_list(lv_obj_t *list)
{
    lv_obj_set_style_bg_color(list, lv_color_hex(0xF8F9FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 2, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
}

void style_list_header(lv_obj_t *label)
{
    style_text(label, 0x697386);
    lv_obj_set_style_bg_color(label, lv_color_hex(0xF8F9FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_left(label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 2, LV_PART_MAIN);
}

void style_list_row(lv_obj_t *row)
{
    lv_obj_set_height(row, 40);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xEFF6FF), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(row, 8, LV_PART_MAIN);
    lv_obj_set_style_text_font(row, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(row, lv_color_hex(0x222B3A), LV_PART_MAIN);
}

lv_obj_t *create_header(lv_obj_t *page, const char *title, lv_event_cb_t back_cb, lv_event_cb_t action_cb)
{
    lv_obj_t *header = lv_obj_create(page);
    lv_obj_set_size(header, 304, 42);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    if (back_cb != nullptr) {
        lv_obj_t *back_btn = lv_button_create(header);
        lv_obj_set_size(back_btn, 48, 34);
        lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t *back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        style_text(back_label, 0x222B3A);
        lv_obj_center(back_label);
    }

    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    style_text(title_label, 0x141922);
    lv_obj_center(title_label);

    if (action_cb != nullptr) {
        lv_obj_t *action_btn = lv_button_create(header);
        lv_obj_set_size(action_btn, 48, 34);
        lv_obj_align(action_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(action_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(action_btn, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(action_btn, action_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t *action_label = lv_label_create(action_btn);
        lv_label_set_text(action_label, LV_SYMBOL_REFRESH);
        style_text(action_label, 0x2266DD);
        lv_obj_center(action_label);
    }

    return header;
}

lv_obj_t *create_primary_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 288, 38);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2266DD), LV_PART_MAIN);
    lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    style_text(label, 0xFFFFFF);
    lv_obj_center(label);
    return button;
}

lv_obj_t *create_detail_panel(lv_obj_t *page)
{
    lv_obj_t *panel = lv_obj_create(page);
    lv_obj_set_size(panel, 304, 330);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 14, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

} // namespace settings_ui
