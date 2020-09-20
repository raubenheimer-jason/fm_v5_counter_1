#include "wifi.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "WIFI";

static const uint32_t wifi_max_disconnected_count = 3600; // roughly 1 per second // maximum number of times the wifi doesn't connect (consecutively) before restarting the device

// Static prototypes
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static uint32_t wifi_disconnected_count = 0;

    printf("_________________________ EVENT HANDLER _________________________\n");
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_disconnected_count++;
        ESP_LOGE(TAG, "** retry to connect to the AP ** (wifi_disconnected_count = %d)", wifi_disconnected_count);

        if (wifi_disconnected_count > wifi_max_disconnected_count)
        {
            ESP_LOGE(TAG, "wifi_disconnected_count > wifi_max_disconnected_count, setting restart_required_flag = true  (wifi_max_disconnected_count = %d)", wifi_max_disconnected_count);
            restart_required_flag = true;

            if (wifi_disconnected_count > wifi_max_disconnected_count * 2)
            {
                ESP_LOGE(TAG, "restart_required_flag no successful, forcing restart");
                esp_restart();
            }
        }

        if (on_mains_flag == 1) // only turn LED on if on mains power
        {
            gpio_set_level(CONFIG_WIFI_LED_PIN, 1);
        }

        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        gpio_set_level(CONFIG_WIFI_LED_PIN, 0);

        wifi_disconnected_count = 0; // reset disconnected count when wifi connects

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    //Initialize NVS (for WiFi)

    // esp_err_t erase_ret = nvs_flash_erase();
    // printf("erase_ret: %s\n", esp_err_to_name(erase_ret));

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_t instance_sta_disconnect;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        WIFI_EVENT_STA_DISCONNECTED,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_sta_disconnect));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            // .threshold.rssi = -100,

            // .scan_method = WIFI_ALL_CHANNEL_SCAN,

            // .bssid_set = false,

            // .channel = 0,

            // .listen_interval = 0,

            // .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
