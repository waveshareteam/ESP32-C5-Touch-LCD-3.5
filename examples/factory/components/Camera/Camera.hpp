#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
namespace esp_brookesia::apps {
class Camera : public systems::phone::App, private AppWorker {
public:
    static Camera *requestInstance();
    ~Camera() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Camera();
    static Camera *instance_;
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    void work() override;
    void refresh();
    void save(bsp_camera_frame_t *);
    void message(const char *);
    SemaphoreHandle_t mutex_=nullptr;
    std::atomic<bool> capture_{false};
    bool preview_=false;
    uint32_t button_presses_=0;
    char message_[96]="Starting camera...";
    lv_obj_t *root_=nullptr,*status_=nullptr;
    lv_timer_t *timer_=nullptr;
};
}
