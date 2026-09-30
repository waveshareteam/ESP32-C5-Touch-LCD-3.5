#include "settings_power_page.hpp"

#include "system_manage_service.h"
#include "settings_page_common.hpp"

namespace {

static lv_obj_t *s_power_info = nullptr;
static lv_timer_t *s_power_timer = nullptr;

constexpr uint32_t POWER_REFRESH_PERIOD_MS = 1000;

static void power_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    settings_power_page_refresh();
}

static void refresh_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        settings_power_page_refresh();
    }
}

static void power_page_delete_event_cb(lv_event_t *e)
{
    (void)e;
    settings_power_page_stop();
    s_power_info = nullptr;
}

} // namespace

void settings_power_page_refresh(void)
{
    if (s_power_info == nullptr) {
        return;
    }

    axp2101_power_info_t state = {};
    if (!system_manage_service_get_power_info(&state)) {
        lv_label_set_text(s_power_info, "AXP2101 is not available");
        return;
    }

    lv_label_set_text_fmt(s_power_info,
                          "Chip ID: 0x%02X\n"
                          "Battery: %s\n"
                          "Battery level: %d%%\n"
                          "Battery voltage: %u mV\n"
                          "VBUS: %s, %u mV\n"
                          "System voltage: %u mV\n"
                          "Charging: %s",
                          state.chip_id,
                          state.battery_connected ? "connected" : "not connected",
                          state.battery_percent,
                          state.battery_voltage_mv,
                          state.vbus_present ? "present" : "not present",
                          state.vbus_voltage_mv,
                          state.system_voltage_mv,
                          state.charging ? "yes" : "no");
}

void settings_power_page_start(void)
{
    settings_power_page_refresh();
    if (s_power_timer == nullptr) {
        s_power_timer = lv_timer_create(power_timer_cb, POWER_REFRESH_PERIOD_MS, nullptr);
    }
}

void settings_power_page_stop(void)
{
    if (s_power_timer != nullptr) {
        lv_timer_delete(s_power_timer);
        s_power_timer = nullptr;
    }
}

lv_obj_t *settings_power_page_create(lv_obj_t *parent, lv_event_cb_t back_cb)
{
    lv_obj_t *page = lv_obj_create(parent);
    settings_ui::style_sheet(page);
    settings_ui::create_header(page, "Power", back_cb, refresh_event_cb);
    lv_obj_add_event_cb(page, power_page_delete_event_cb, LV_EVENT_DELETE, nullptr);

    lv_obj_t *panel = settings_ui::create_detail_panel(page);
    s_power_info = lv_label_create(panel);
    settings_ui::style_text(s_power_info, 0x222B3A);
    lv_obj_set_width(s_power_info, 276);
    lv_label_set_long_mode(s_power_info, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_power_info, "Reading AXP2101...");
    lv_obj_align(s_power_info, LV_ALIGN_TOP_LEFT, 0, 0);
    return page;
}
