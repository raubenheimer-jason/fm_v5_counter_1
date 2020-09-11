#include "mqtt.h"

char device_id[20];

static const char *TAG = "MQTT";

/**
 * Function to initialise MQTT
 * 
 * Make sure NVS is already initialised
 * 
 * Cert is the "minimal root CA" found: https://cloud.google.com/iot/docs/how-tos/mqtt-bridge
 */
void mqtt_init(void)
{
    esp_err_t res = get_device_id(device_id);

    printf("res: %d, dev id: %s\n", res, device_id);

    ESP_ERROR_CHECK(esp_netif_init());
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        gpio_set_level(CONFIG_MQTT_LED_PIN, 1);

        char *sub_topic_config = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/config") + 1); // Check for error allocating memory
        sub_topic_config[0] = '\0';
        strcat(sub_topic_config, "/devices/");
        strcat(sub_topic_config, device_id);
        strcat(sub_topic_config, "/config");

        msg_id = esp_mqtt_client_subscribe(client, sub_topic_config, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        free(sub_topic_config);

        break;
    case MQTT_EVENT_DISCONNECTED:
        gpio_set_level(CONFIG_MQTT_LED_PIN, 0);
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }

    return ESP_OK;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

/**
 * Check if the JWT needs to be updated
 * 
 * @returns: true if update is necessary
 */
bool jwt_update_check(void)
{
    static uint32_t last_updated = 0;

    time_t now;
    time(&now);

    while (now < CONFIG_LAST_KNOWN_UNIX)
    {
        ESP_LOGW(TAG, "waiting for time to be updated before updating JWT");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        printf("now: %ld\n", now);
    }

    if ((last_updated + ((float)CONFIG_JWT_EXP - (0.1 * CONFIG_JWT_EXP))) < now)
    {
        printf("the value: %f   now:%d\n", (last_updated + ((float)CONFIG_JWT_EXP - (0.1 * CONFIG_JWT_EXP))), (uint32_t)now);
        ESP_LOGI(TAG, "need to update JWT");
        last_updated = now;
        return true;
    }

    return false;
}

/**
 * Generates the Device ID from the MAC address
 */
esp_err_t get_device_id(char device_id[])
{
    uint8_t raw_mac[6];

    esp_err_t res = esp_efuse_mac_get_default(raw_mac);

    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_efuse_mac_get_default failed\n");
        return res;
    }

    // sprintf(device_id, "C-%02X%02X%02X%02X%02X%02X", raw_mac[0], raw_mac[1], raw_mac[2], raw_mac[3], raw_mac[4], raw_mac[5]); // KEEP THIS !!!!!!!!!!!!!

    sprintf(device_id, "new-test-device"); //new-test-device

    return ESP_OK;
}