#include "Sensor.hpp"
#include <cstdio>
#include <new>
#include "esp_timer.h"
LV_IMAGE_DECLARE(img_app_sensor);
using namespace app_ui;
namespace esp_brookesia::apps {
Sensor *Sensor::instance_;
Sensor *Sensor::requestInstance() { if(!instance_) instance_=new(std::nothrow) Sensor; return instance_; }
Sensor::Sensor():App("Sensor",&img_app_sensor,true,false,false) {}
Sensor::~Sensor() { close(); instance_=nullptr; }
bool Sensor::run() {
    root_=screen("Sensors",event,this);
    auto *rtc=box(root_,12,72,296,92);
    label(rtc,"PCF85063  /  RTC",16,8,264,&lv_font_montserrat_14,MUTED);
    clock_=label(rtc,"--:--:--",16,30,264,&lv_font_montserrat_32);
    date_=label(rtc,"--",16,72,264,&lv_font_montserrat_12,MUTED);
    auto *environment=box(root_,12,174,296,96);
    label(environment,"SHTC3  /  ENVIRONMENT",16,10,264,&lv_font_montserrat_14,MUTED);
    label(environment,"Temperature",16,36,128,&lv_font_montserrat_12,MUTED);
    label(environment,"Humidity",152,36,128,&lv_font_montserrat_12,MUTED);
    temperature_=label(environment,"-- C",16,58,128,&lv_font_montserrat_24);
    humidity_=label(environment,"-- %RH",152,58,128,&lv_font_montserrat_24);
    auto *imu=box(root_,12,280,296,156);
    label(imu,"QMI8658  /  MOTION",16,10,264,&lv_font_montserrat_14,MUTED);
    label(imu,"Accel (g)",16,36,128,&lv_font_montserrat_14,BLUE);
    label(imu,"Gyro (deg/s)",152,36,128,&lv_font_montserrat_14,0xd07a25);
    for(int i=0;i<6;++i) values_[i]=label(imu,"--",16+(i/3)*136,62+(i%3)*30,128,&lv_font_montserrat_18);
    status_=label(root_,"Reading sensors...",14,448,292,&lv_font_montserrat_12,MUTED);
    timer_=lv_timer_create(poll,100,this);
    if(!timer_ || !start("sensor",4096)) { close(); return false; }
    return true;
}
void Sensor::work() {
    int64_t next_rtc=0,last_imu=0;
    while(!stopping()) {
        auto now=esp_timer_get_time();
        if(now>=next_rtc) {
            bsp_sensor_datetime_t t={}; auto err=bsp_rtc_get_datetime(&t);
            portENTER_CRITICAL(&lock_); rtc_error_=err; if(err==ESP_OK) time_=t; portEXIT_CRITICAL(&lock_);
            bsp_shtc3_data_t environment={}; err=bsp_shtc3_get_data(&environment);
            portENTER_CRITICAL(&lock_); shtc3_error_=err; if(err==ESP_OK) environment_=environment; portEXIT_CRITICAL(&lock_);
            next_rtc=now+1000000;
        }
        bsp_imu_data_t m={}; auto err=bsp_imu_get_data(&m);
        if(err==ESP_OK) last_imu=now;
        if(err!=ESP_ERR_NOT_FINISHED || now-last_imu>1000000) {
            portENTER_CRITICAL(&lock_); imu_error_=err; if(err==ESP_OK) motion_=m; portEXIT_CRITICAL(&lock_);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void Sensor::refresh() {
    bsp_sensor_datetime_t t; bsp_imu_data_t m; bsp_shtc3_data_t environment; esp_err_t rtc,imu,shtc3;
    portENTER_CRITICAL(&lock_);
    t=time_; m=motion_; environment=environment_; rtc=rtc_error_; imu=imu_error_; shtc3=shtc3_error_;
    portEXIT_CRITICAL(&lock_);
    char s[80];
    if(rtc==ESP_OK) {
        snprintf(s,sizeof(s),"%02u:%02u:%02u",t.hour,t.minute,t.second); text(clock_,s);
        snprintf(s,sizeof(s),"%04u-%02u-%02u",t.year,t.month,t.day); text(date_,s);
    } else { text(clock_,"--:--:--"); text(date_,"RTC unavailable"); }
    if(shtc3==ESP_OK) {
        snprintf(s,sizeof(s),"%.1f C",(double)environment.temperature_c); text(temperature_,s);
        snprintf(s,sizeof(s),"%.1f %%RH",(double)environment.humidity_percent); text(humidity_,s);
    } else { text(temperature_,"-- C"); text(humidity_,"-- %RH"); }
    float data[]={m.accel_x,m.accel_y,m.accel_z,m.gyro_x,m.gyro_y,m.gyro_z};
    for(int i=0;i<6;++i) {
        if(imu==ESP_OK) snprintf(s,sizeof(s),"%c  %+.2f","XYZ"[i%3],(double)data[i]);
        else snprintf(s,sizeof(s),"%c  --","XYZ"[i%3]);
        text(values_[i],s);
    }
    snprintf(s,sizeof(s),"RTC: %s   IMU: %s   SHTC3: %s",rtc==ESP_OK?"OK":"--",imu==ESP_OK?"OK":"--",shtc3==ESP_OK?"OK":"--"); text(status_,s);
}
void Sensor::poll(lv_timer_t *t) { static_cast<Sensor *>(lv_timer_get_user_data(t))->refresh(); }
void Sensor::event(lv_event_t *e) { static_cast<Sensor *>(lv_event_get_user_data(e))->back(); }
bool Sensor::pause() { if(timer_) lv_timer_pause(timer_); stop(); return true; }
bool Sensor::resume() { if(timer_) lv_timer_resume(timer_); return start("sensor",4096); }
bool Sensor::close() { stop(); if(timer_) { lv_timer_delete(timer_); timer_=nullptr; } if(root_) lv_obj_delete(root_); root_=nullptr; return true; }
}
