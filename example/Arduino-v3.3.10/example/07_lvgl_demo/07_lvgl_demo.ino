#include <Arduino.h>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_io_interface.h>
#include "Board_IO.h"
#include "Touch_FT6336.h"

#define LCD_MISO 2
#define LCD_MOSI 7
#define LCD_SCLK 6
#define LCD_CS   8
#define LCD_DC   5
#define LCD_WIDTH 320
#define LCD_HEIGHT 480

static esp_lcd_panel_io_handle_t lcd_io = nullptr;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *draw_buffer = nullptr;
static lv_color_t *draw_buffer_2 = nullptr;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static bool touch_ready = false;
static volatile bool lcd_transfer_done = false;
static lv_obj_t *backlight_label = nullptr;
static lv_obj_t *button_label = nullptr;
static uint32_t button_clicks = 0;

static bool lcd_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *) {
  lcd_transfer_done = true;
  return false;
}

static bool lcd_command(uint8_t command, const uint8_t *data = nullptr, size_t length = 0) {
  return esp_lcd_panel_io_tx_param(lcd_io, command, data, length) == ESP_OK;
}

static bool lcd_begin() {
  spi_bus_config_t bus_config = {};
  bus_config.sclk_io_num = LCD_SCLK;
  bus_config.mosi_io_num = LCD_MOSI;
  bus_config.miso_io_num = LCD_MISO;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = LCD_WIDTH * 40 * sizeof(lv_color_t);

  esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("ERROR: SPI bus init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.dc_gpio_num = LCD_DC;
  io_config.cs_gpio_num = LCD_CS;
  io_config.pclk_hz = 60000000;
  io_config.spi_mode = 0;
  io_config.trans_queue_depth = 10;
  io_config.on_color_trans_done = lcd_color_done;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &lcd_io);
  if (err != ESP_OK) {
    Serial.printf("ERROR: LCD panel IO init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  // This is the exact ST7796 sequence used by the working ESP-IDF BSP.
  lcd_command(0x01); // software reset
  delay(20);
  lcd_command(0x11); // sleep out
  delay(100);
  const uint8_t madctl = 0x48; // BGR + mirror X, same as BSP
  const uint8_t colmod = 0x55; // RGB565
  lcd_command(0x36, &madctl, 1);
  lcd_command(0x3A, &colmod, 1);

  const uint8_t d_f0_c3[] = {0xC3}; lcd_command(0xF0, d_f0_c3, sizeof(d_f0_c3));
  const uint8_t d_f0_96[] = {0x96}; lcd_command(0xF0, d_f0_96, sizeof(d_f0_96));
  const uint8_t d_b4[] = {0x01}; lcd_command(0xB4, d_b4, sizeof(d_b4));
  const uint8_t d_b7[] = {0xC6}; lcd_command(0xB7, d_b7, sizeof(d_b7));
  const uint8_t d_e8[] = {0x40,0x8A,0x00,0x00,0x29,0x19,0xA5,0x33}; lcd_command(0xE8, d_e8, sizeof(d_e8));
  const uint8_t d_c1[] = {0x06}; lcd_command(0xC1, d_c1, sizeof(d_c1));
  const uint8_t d_c2[] = {0xA7}; lcd_command(0xC2, d_c2, sizeof(d_c2));
  const uint8_t d_c5[] = {0x18}; lcd_command(0xC5, d_c5, sizeof(d_c5));
  const uint8_t d_e0[] = {0xF0,0x09,0x0B,0x06,0x04,0x15,0x2F,0x54,0x42,0x3C,0x17,0x14,0x18,0x1B}; lcd_command(0xE0, d_e0, sizeof(d_e0));
  const uint8_t d_e1[] = {0xF0,0x09,0x0B,0x06,0x04,0x03,0x2D,0x43,0x42,0x3B,0x16,0x14,0x17,0x1B}; lcd_command(0xE1, d_e1, sizeof(d_e1));
  const uint8_t d_f0_3c[] = {0x3C}; lcd_command(0xF0, d_f0_3c, sizeof(d_f0_3c));
  const uint8_t d_f0_69[] = {0x69}; lcd_command(0xF0, d_f0_69, sizeof(d_f0_69));
  lcd_command(0x21); // inversion on, same as BSP
  lcd_command(0x29); // display on
  delay(120);
  return true;
}

static bool lcd_send_area(int x1, int y1, int x2, int y2, const void *pixels, size_t bytes) {
  uint8_t column[] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
  uint8_t row[] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
  if (!lcd_command(0x2A, column, sizeof(column)) || !lcd_command(0x2B, row, sizeof(row))) return false;
  lcd_transfer_done = false;
  if (esp_lcd_panel_io_tx_color(lcd_io, 0x2C, pixels, bytes) != ESP_OK) return false;
  uint32_t started = millis();
  while (!lcd_transfer_done && millis() - started < 1000) delay(1);
  return lcd_transfer_done;
}

static void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  size_t bytes = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(lv_color_t);
  if (!lcd_send_area(area->x1, area->y1, area->x2, area->y2, color_p, bytes)) {
    Serial.println("ERROR: LVGL LCD transfer failed");
  }
  lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *, lv_indev_data_t *data) {
  static uint16_t last_x = LCD_WIDTH / 2;
  static uint16_t last_y = LCD_HEIGHT / 2;
  uint16_t x[2] = {}, y[2] = {};
  uint8_t count = 0;
  if (touch_ready && FT6336_Read(x, y, &count, 1) && count &&
      x[0] < LCD_WIDTH && y[0] < LCD_HEIGHT) {
    last_x = x[0];
    last_y = y[0];
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  data->point.x = last_x;
  data->point.y = last_y;
  data->continue_reading = false;
}

static void button_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED) return;
  button_clicks++;
  lv_label_set_text_fmt(button_label, "Clicked: %u", (unsigned)button_clicks);
}

static void slider_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
  int32_t value = lv_slider_get_value(lv_event_get_target(event));
  Board_SetBacklight((uint8_t)value);
  lv_label_set_text_fmt(backlight_label, "Backlight: %d%%", (int)value);
}

static void make_ui() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x10233A), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x24557A), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "ESP32-C5 Touch LCD 3.5");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  lv_obj_t *subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "Arduino Core 3.3.10 + LVGL v8");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0x8DE7F5), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 58);

  lv_obj_t *panel = lv_obj_create(screen);
  lv_obj_set_size(panel, 280, 270);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_set_style_radius(panel, 14, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *display_status = lv_label_create(panel);
  lv_label_set_text(display_status, "Display: ST7796 320 x 480");
  lv_obj_set_style_text_color(display_status, lv_color_hex(0x18324A), 0);
  lv_obj_align(display_status, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *touch_status = lv_label_create(panel);
  lv_label_set_text(touch_status, "Touch: FT6336 ready");
  lv_obj_set_style_text_color(touch_status, lv_color_hex(0x18324A), 0);
  lv_obj_align(touch_status, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *button = lv_btn_create(panel);
  lv_obj_set_size(button, 180, 58);
  lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 72);
  button_label = lv_label_create(button);
  lv_label_set_text(button_label, "Touch test");
  lv_obj_center(button_label);
  lv_obj_add_event_cb(button, button_event, LV_EVENT_PRESSED, nullptr);

  backlight_label = lv_label_create(panel);
  lv_label_set_text(backlight_label, "Backlight: 100%");
  lv_obj_set_style_text_color(backlight_label, lv_color_hex(0x18324A), 0);
  lv_obj_align(backlight_label, LV_ALIGN_TOP_MID, 0, 155);

  lv_obj_t *slider = lv_slider_create(panel);
  lv_obj_set_size(slider, 220, 18);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 198);
  lv_slider_set_range(slider, 10, 100);
  lv_slider_set_value(slider, 100, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, slider_event, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *footer = lv_label_create(screen);
  lv_obj_set_width(footer, 290);
  lv_label_set_long_mode(footer, LV_LABEL_LONG_WRAP);
  lv_label_set_text(footer, "LVGL rendering and touch work\nindependently of the SD card");
  lv_obj_set_style_text_color(footer, lv_color_hex(0xD7F6FF), 0);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32-C5-Touch-LCD-3.5 ESP-IDF LCD IO + LVGL v8 demo");

  if (!Board_IO_Init(Wire)) {
    Serial.println("ERROR: CH32V003 init failed");
    while (true) delay(1000);
  }
  Board_SetBacklight(0);

  // The working IDF BSP resets touch/LCD before panel initialization.  Do not
  // toggle either expander reset output after ST7796 has been configured.
  touch_ready = FT6336_Init();
  Board_LCD_Reset();

  if (!lcd_begin()) {
    Serial.println("ERROR: IDF-compatible ST7796 init failed");
    while (true) delay(1000);
  }
  constexpr size_t buffer_pixels = LCD_WIDTH * 40;
  draw_buffer = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  draw_buffer_2 = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!draw_buffer || !draw_buffer_2) {
    Serial.println("ERROR: LVGL draw buffer allocation failed");
    while (true) delay(1000);
  }
  Board_SetBacklight(100);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, draw_buffer, draw_buffer_2, buffer_pixels);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_t *display = lv_disp_drv_register(&disp_drv);

  if (touch_ready) {
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);
  }

  make_ui();
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(display);
  Serial.println("ESP-IDF LCD IO + LVGL demo started");
}

void loop() {
  lv_tick_inc(5);
  lv_timer_handler();
  delay(5);
}
