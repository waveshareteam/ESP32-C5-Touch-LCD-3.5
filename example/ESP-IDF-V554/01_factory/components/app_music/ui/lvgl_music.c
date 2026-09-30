#include "lvgl_music.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp_board_extra.h"
#include "esp_log.h"

static const char *TAG = "music_ui";

LV_IMAGE_DECLARE(img_app_music_cover_1);

static const char *const s_track_names[] = {
    "cut_test0.mp3", "cut_test1.mp3", "cut_test2.mp3",
};
static const char *const s_track_uris[] = {
    "file://spiffs/cut_test0.mp3",
    "file://spiffs/cut_test1.mp3",
    "file://spiffs/cut_test2.mp3",
};

#define TRACK_COUNT (sizeof(s_track_uris) / sizeof(s_track_uris[0]))

static lv_obj_t *s_root;
static lv_obj_t *s_track_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_play_label;
static lv_timer_t *s_state_timer;
static uint8_t s_track_index;
static bool s_player_initialized;
static bool s_track_started;
static bool s_is_playing;

static void update_ui(void)
{
    if (s_track_label) {
        lv_label_set_text(s_track_label, s_track_names[s_track_index]);
    }
    if (s_state_label) {
        lv_label_set_text(s_state_label, s_is_playing ? "Playing" :
                          (s_track_started ? "Paused" : "Ready"));
    }
    if (s_play_label) {
        lv_label_set_text(s_play_label, s_is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

void music_player_init(void)
{
    if (!s_player_initialized) {
        s_player_initialized = Audio_Play_Init() != NULL;
    }
}

static bool play_selected(void)
{
    music_player_init();
    if (!s_player_initialized) {
        lv_label_set_text(s_state_label, "Audio init failed");
        return false;
    }

    esp_gmf_err_t ret = Audio_Play_Music(s_track_uris[s_track_index]);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Play %s failed: %d", s_track_uris[s_track_index], ret);
        s_track_started = false;
        s_is_playing = false;
        lv_label_set_text(s_state_label, "MP3 unavailable");
        return false;
    }
    s_track_started = true;
    s_is_playing = true;
    update_ui();
    return true;
}

static void previous_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    s_track_index = (s_track_index + TRACK_COUNT - 1) % TRACK_COUNT;
    if (s_track_started) play_selected();
    else update_ui();
}

static void next_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    s_track_index = (s_track_index + 1) % TRACK_COUNT;
    if (s_track_started) play_selected();
    else update_ui();
}

static void play_pause_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_is_playing) {
        if (Audio_Pause_Play() == ESP_GMF_ERR_OK) {
            s_is_playing = false;
            update_ui();
        }
    } else if (s_track_started) {
        if (Audio_Resume_Play() == ESP_GMF_ERR_OK) {
            s_is_playing = true;
            update_ui();
        }
    } else {
        play_selected();
    }
}

static lv_obj_t *create_control(lv_obj_t *parent, const char *symbol,
                                lv_event_cb_t callback, bool primary)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, primary ? 68 : 56, primary ? 68 : 56);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(primary ? 0x2266DD : 0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(primary ? 0xFFFFFF : 0x202632), LV_PART_MAIN);
    lv_obj_center(label);
    return label;
}

static void state_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_track_started) return;

    esp_asp_state_t state = Audio_Get_Current_State();
    if (state == ESP_ASP_STATE_FINISHED || state == ESP_ASP_STATE_STOPPED ||
        state == ESP_ASP_STATE_ERROR) {
        s_track_started = false;
        s_is_playing = false;
        update_ui();
    }
}

void music_ui_create(lv_obj_t *parent)
{
    music_ui_destroy();

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, 320, 480);
    lv_obj_center(s_root);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0xF5F6F8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "Music");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x202632), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* Album-art frame. */
    lv_obj_t *cover = lv_obj_create(s_root);
    lv_obj_set_size(cover, 190, 190);
    lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(cover, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(cover, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cover, lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_style_radius(cover, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cover, 0, LV_PART_MAIN);
    lv_obj_remove_flag(cover, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cover_image = lv_image_create(cover);
    lv_image_set_src(cover_image, &img_app_music_cover_1);
    lv_obj_center(cover_image);

    s_track_label = lv_label_create(s_root);
    lv_obj_set_width(s_track_label, 280);
    lv_label_set_long_mode(s_track_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_track_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_track_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_track_label, lv_color_hex(0x202632), LV_PART_MAIN);
    lv_obj_align(s_track_label, LV_ALIGN_TOP_MID, 0, 246);

    s_state_label = lv_label_create(s_root);
    lv_obj_set_style_text_color(s_state_label, lv_color_hex(0x6B7280), LV_PART_MAIN);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_MID, 0, 272);

    lv_obj_t *controls = lv_obj_create(s_root);
    lv_obj_set_size(controls, 270, 80);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(controls, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(controls, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(controls, 22, LV_PART_MAIN);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    create_control(controls, LV_SYMBOL_PREV, previous_cb, false);
    s_play_label = create_control(controls, LV_SYMBOL_PLAY, play_pause_cb, true);
    create_control(controls, LV_SYMBOL_NEXT, next_cb, false);

    s_track_index = 0;
    s_track_started = false;
    s_is_playing = false;
    update_ui();
    s_state_timer = lv_timer_create(state_timer_cb, 500, NULL);
}

void music_ui_stop(void)
{
    if (s_track_started) Audio_Stop_Play();
    s_track_started = false;
    s_is_playing = false;
    update_ui();
}

void music_ui_pause(void)
{
    if (s_is_playing && Audio_Pause_Play() == ESP_GMF_ERR_OK) {
        s_is_playing = false;
        update_ui();
    }
}

void music_ui_resume(void)
{
    if (s_track_started && !s_is_playing && Audio_Resume_Play() == ESP_GMF_ERR_OK) {
        s_is_playing = true;
        update_ui();
    }
}

void music_ui_destroy(void)
{
    if (s_state_timer) {
        lv_timer_delete(s_state_timer);
        s_state_timer = NULL;
    }
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
    s_track_label = NULL;
    s_state_label = NULL;
    s_play_label = NULL;
}
