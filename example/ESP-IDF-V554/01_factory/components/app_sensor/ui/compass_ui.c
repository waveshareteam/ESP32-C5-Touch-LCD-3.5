#include "compass_ui.h"
#include "system_manage_service.h"

static const char *TAG = "qmi8658";
static qmi8658_dev_t *qmi8658_dev = NULL;
static i2c_master_dev_handle_t shtc3_dev = NULL;
static lv_obj_t *labels_qmi8658_data[8];
static lv_obj_t *labels_shtc3_data[2];
static sys_manage_service_t *sms = NULL;
static lv_timer_t *timer = NULL;
static void example1_increase_lvgl_tick(lv_timer_t * t);


static lv_obj_t *con = NULL;
static char buf[10][16];



static void style_panel(lv_obj_t *panel)
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x20242B), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x343A46), 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *create_sensor_label(lv_obj_t *parent, int x, int y,
                                     const char *name, const char *value,
                                     lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, 132, 40);
    lv_label_set_text_fmt(label, "%s\n%s", name, value);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_line_space(label, 2, 0);
    return label;
}

// 主初始化函数
void sensor_layout_init(void)
{
    sms =  get_system_manage_service_handle();
    qmi8658_dev = sms->qmi8658_dev;
    shtc3_dev = sms->shtc3_dev;

    if (con != NULL) {
        return;
    }

    con = lv_obj_create(lv_screen_active());
    lv_obj_set_size(con, 320, 404);
    lv_obj_align(con, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(con, lv_color_hex(0x111318), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(con, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(con, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(con, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(con, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(con, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(con, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *page_title = lv_label_create(con);
    lv_label_set_text(page_title, "Sensors");
    lv_obj_set_style_text_color(page_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(page_title, &lv_font_montserrat_14, 0);
    lv_obj_align(page_title, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t *cont_qmi = lv_obj_create(con);
    lv_obj_set_pos(cont_qmi, 12, 42);
    lv_obj_set_size(cont_qmi, 296, 238);
    style_panel(cont_qmi);

    lv_obj_t *title_qmi = lv_label_create(cont_qmi);
    lv_label_set_text(title_qmi, "QMI8658  Motion");
    lv_obj_set_style_text_color(title_qmi, lv_color_hex(0xC7CDD8), 0);
    lv_obj_set_style_text_font(title_qmi, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(title_qmi, 12, 10);

    labels_qmi8658_data[0] = create_sensor_label(cont_qmi, 12, 42,  "Accel X", "--.--", lv_color_hex(0x4DDDE0));
    labels_qmi8658_data[4] = create_sensor_label(cont_qmi, 152, 42, "Gyro X",  "--.--", lv_color_hex(0xFFB84D));
    labels_qmi8658_data[1] = create_sensor_label(cont_qmi, 12, 88,  "Accel Y", "--.--", lv_color_hex(0x4DDDE0));
    labels_qmi8658_data[5] = create_sensor_label(cont_qmi, 152, 88, "Gyro Y",  "--.--", lv_color_hex(0xFFB84D));
    labels_qmi8658_data[2] = create_sensor_label(cont_qmi, 12, 134, "Accel Z", "--.--", lv_color_hex(0x4DDDE0));
    labels_qmi8658_data[6] = create_sensor_label(cont_qmi, 152, 134,"Gyro Z",  "--.--", lv_color_hex(0xFFB84D));
    labels_qmi8658_data[3] = create_sensor_label(cont_qmi, 12, 180, "IMU Temp", "--.-- C", lv_color_hex(0x70DF8A));
    labels_qmi8658_data[7] = create_sensor_label(cont_qmi, 152, 180,"Timestamp", "---- ms", lv_color_hex(0xB99AFF));

    lv_obj_t *cont_shtc = lv_obj_create(con);
    lv_obj_set_pos(cont_shtc, 12, 288);
    lv_obj_set_size(cont_shtc, 296, 104);
    style_panel(cont_shtc);

    lv_obj_t *title_shtc = lv_label_create(cont_shtc);
    lv_label_set_text(title_shtc, "SHTC3  Environment");
    lv_obj_set_style_text_color(title_shtc, lv_color_hex(0xC7CDD8), 0);
    lv_obj_set_style_text_font(title_shtc, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(title_shtc, 12, 9);

    labels_shtc3_data[0] = create_sensor_label(cont_shtc, 12, 40, "Temperature", "--.-- C", lv_color_hex(0x70DF8A));
    labels_shtc3_data[1] = create_sensor_label(cont_shtc, 152, 40, "Humidity", "--.-- %%", lv_color_hex(0x66A8FF));

    timer = lv_timer_create(example1_increase_lvgl_tick, 1000, NULL);
}


char datetime_str[256];
static void example1_increase_lvgl_tick(lv_timer_t * t)
{
    qmi8658_data_t data;
    bool ready;
    float shtc_temp, shtc_humi;
    shtc3_register_rw_t reg = SHTC3_REG_T_CSD_NM;

    // 读取 QMI8658
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    if (qmi8658_dev != NULL) {
        ret = qmi8658_is_data_ready(qmi8658_dev, &ready);
    }
    if (ret == ESP_OK && ready) {
        ret = qmi8658_read_sensor_data(qmi8658_dev, &data);
        if (ret == ESP_OK) {
            sprintf(buf[0], "%.2f", data.accelX);
            sprintf(buf[1], "%.2f", data.accelY);
            sprintf(buf[2], "%.2f", data.accelZ);
            sprintf(buf[3], "%.2f", data.temperature);
            sprintf(buf[4], "%.2f", data.gyroX);
            sprintf(buf[5], "%.2f", data.gyroY);
            sprintf(buf[6], "%.2f", data.gyroZ);

            lv_label_set_text_fmt(labels_qmi8658_data[0], "Accel X\n%s", buf[0]);
            lv_label_set_text_fmt(labels_qmi8658_data[1], "Accel Y\n%s", buf[1]);
            lv_label_set_text_fmt(labels_qmi8658_data[2], "Accel Z\n%s", buf[2]);
            lv_label_set_text_fmt(labels_qmi8658_data[3], "IMU Temp\n%s C", buf[3]);
            lv_label_set_text_fmt(labels_qmi8658_data[4], "Gyro X\n%s", buf[4]);
            lv_label_set_text_fmt(labels_qmi8658_data[5], "Gyro Y\n%s", buf[5]);
            lv_label_set_text_fmt(labels_qmi8658_data[6], "Gyro Z\n%s", buf[6]);
            lv_label_set_text_fmt(labels_qmi8658_data[7], "Timestamp\n%lu ms", data.timestamp);
        }
    }

    // 读取 SHTC3
    shtc3_get_th(shtc3_dev, reg, &shtc_temp, &shtc_humi);
    sprintf(buf[8], "%.2f", shtc_temp);
    sprintf(buf[9], "%.2f", shtc_humi);

    lv_label_set_text_fmt(labels_shtc3_data[0], "Temperature\n%s C", buf[8]);
    lv_label_set_text_fmt(labels_shtc3_data[1], "Humidity\n%s %%", buf[9]);

    //ESP_LOGI("TAG", "SHTC3 → %.2f°C, %.2f%%RH", shtc_temp, shtc_humi);


    // Read current time from RTC
    pcf85063a_datetime_t Now_time;
    pcf85063a_get_time_date(sms->pcf85063a_dev, &Now_time);

    // Format current time as a string
    pcf85063a_datetime_to_str(datetime_str, Now_time);
    ESP_LOGI(TAG, "Now_time is %s", datetime_str);
}


void sensor_layout_deinit(void)
{
    if (timer != NULL) {
        lv_timer_delete(timer);
        timer = NULL;
    }
    if (con != NULL) {
        lv_obj_delete(con);
        con = NULL;
    }
}
