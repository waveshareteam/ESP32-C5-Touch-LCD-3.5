#include "wifi_manager.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     3

static const char *TAG = "wifi_manager";

typedef struct {
    wifi_mgr_mode_t     mode;
    bool                connected;
    int                 retry_count;
    bool                scan_requested;
    EventGroupHandle_t  event_group;
    esp_netif_t        *netif;
    wifi_mgr_callback_t callbacks[WIFI_MGR_MAX_CALLBACKS];
    int                 callback_count;
} wifi_manager_t;

static wifi_manager_t s_mgr = {0};

static void _notify_callbacks(wifi_mgr_event_t event, void *event_data)
{
    for (int i = 0; i < s_mgr.callback_count; i++) {
        if (s_mgr.callbacks[i]) {
            s_mgr.callbacks[i](event, event_data);
        }
    }
}

static void _event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_mgr.connected = false;
        if (s_mgr.scan_requested) {
            ESP_LOGI(TAG, "STA reconnect paused for user Wi-Fi scan");
            _notify_callbacks(WIFI_MGR_EVENT_DISCONNECTED, event_data);
            return;
        }
        if (s_mgr.retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_mgr.retry_count++;
            ESP_LOGW(TAG, "Retrying... (%d/%d)", s_mgr.retry_count, WIFI_MAX_RETRY);
            _notify_callbacks(WIFI_MGR_EVENT_DISCONNECTED, event_data);
        } else {
            xEventGroupSetBits(s_mgr.event_group, WIFI_FAIL_BIT);
            _notify_callbacks(WIFI_MGR_EVENT_FAIL, event_data);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_mgr.retry_count = 0;
        s_mgr.connected = true;
        s_mgr.scan_requested = false;
        xEventGroupSetBits(s_mgr.event_group, WIFI_CONNECTED_BIT);
        _notify_callbacks(WIFI_MGR_EVENT_CONNECTED, event_data);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "A station connected to SoftAP");
        _notify_callbacks(WIFI_MGR_EVENT_AP_STA_CONNECTED, event_data);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "A station disconnected from SoftAP");
        _notify_callbacks(WIFI_MGR_EVENT_AP_STA_DISCONNECTED, event_data);
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_mgr.event_group    = xEventGroupCreate();
    s_mgr.mode           = WIFI_MGR_MODE_NONE;
    s_mgr.connected      = false;
    s_mgr.retry_count    = 0;
    s_mgr.scan_requested = false;
    s_mgr.callback_count = 0;
    memset(s_mgr.callbacks, 0, sizeof(s_mgr.callbacks));

    ESP_LOGI(TAG, "wifi_manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_register_callback(wifi_mgr_callback_t cb)
{
    if (cb == NULL) return ESP_ERR_INVALID_ARG;
    if (s_mgr.callback_count >= WIFI_MGR_MAX_CALLBACKS) {
        ESP_LOGE(TAG, "Callback list full");
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < s_mgr.callback_count; i++) {
        if (s_mgr.callbacks[i] == cb) return ESP_OK;
    }
    s_mgr.callbacks[s_mgr.callback_count++] = cb;
    ESP_LOGI(TAG, "Callback registered (%d total)", s_mgr.callback_count);
    return ESP_OK;
}

esp_err_t wifi_manager_unregister_callback(wifi_mgr_callback_t cb)
{
    for (int i = 0; i < s_mgr.callback_count; i++) {
        if (s_mgr.callbacks[i] == cb) {
            for (int j = i; j < s_mgr.callback_count - 1; j++) {
                s_mgr.callbacks[j] = s_mgr.callbacks[j + 1];
            }
            s_mgr.callbacks[--s_mgr.callback_count] = NULL;
            ESP_LOGI(TAG, "Callback unregistered (%d remaining)", s_mgr.callback_count);
            return ESP_OK;
        }
    }
    ESP_LOGW(TAG, "Callback not found");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_manager_start_sta(const char *ssid, const char *password)
{
    if (s_mgr.mode != WIFI_MGR_MODE_NONE) {
        ESP_LOGE(TAG, "Wi-Fi already started. Call wifi_manager_stop() first.");
        return ESP_ERR_INVALID_STATE;
    }

    s_mgr.netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
            ESP_EVENT_ANY_ID, &_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
            IP_EVENT_STA_GOT_IP, &_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Limit RF current peaks on the shared LCD supply.  Units are 0.25 dBm,
     * so 40 selects 10 dBm instead of the default maximum near 20 dBm. */
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));

    s_mgr.mode = WIFI_MGR_MODE_STA;
    s_mgr.scan_requested = false;
    ESP_LOGI(TAG, "STA started, connecting to: %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(const char *ssid, const char *password,
                                 uint8_t channel, uint8_t max_conn)
{
    if (s_mgr.mode != WIFI_MGR_MODE_NONE) {
        ESP_LOGE(TAG, "Wi-Fi already started. Call wifi_manager_stop() first.");
        return ESP_ERR_INVALID_STATE;
    }

    s_mgr.netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
            ESP_EVENT_ANY_ID, &_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .channel        = channel,
            .max_connection = max_conn,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg        = { .required = true },
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = (uint8_t)strlen(ssid);
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_mgr.mode = WIFI_MGR_MODE_AP;
    s_mgr.scan_requested = false;
    ESP_LOGI(TAG, "SoftAP started. SSID:%s channel:%d", ssid, channel);
    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    if (s_mgr.mode == WIFI_MGR_MODE_NONE) return ESP_OK;

    if (s_mgr.mode == WIFI_MGR_MODE_STA) {
        esp_wifi_disconnect();
    }

    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());

    // 必须在 destroy 之前清理 driver 和 handlers
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_mgr.netif));
    esp_netif_destroy(s_mgr.netif);
    s_mgr.netif = NULL;

    s_mgr.mode = WIFI_MGR_MODE_NONE;
    s_mgr.connected = false;
    s_mgr.retry_count = 0;
    s_mgr.scan_requested = false;
    xEventGroupClearBits(s_mgr.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_LOGI(TAG, "Wi-Fi stopped");
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_mgr.connected;
}

wifi_mgr_mode_t wifi_manager_get_mode(void)
{
    return s_mgr.mode;
}

esp_err_t wifi_manager_prepare_scan(void)
{
    if (s_mgr.mode != WIFI_MGR_MODE_STA || s_mgr.connected) {
        return ESP_OK;
    }

    s_mgr.scan_requested = true;
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_ERR_WIFI_NOT_STARTED || ret == ESP_ERR_WIFI_NOT_INIT) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        s_mgr.scan_requested = false;
    }
    return ret;
}
