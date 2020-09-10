#include "mqtt.h"

// const uint32_t jwt_exp = 3600;

const char *private_key =
    "ab:05:1e:33:36:d9:b0:1e:b2:00:1a:b2:da:1c:21:"
    "84:bf:ee:46:5e:3a:7d:3f:11:1f:73:a6:4b:bd:d7:"
    "5d:f6";

// const char *private_key asm("_binary_device_private_key_txt_start");
// extern const uint8_t wifi_ssid_from_file[] asm("_binary_device_private_key_start");

static const char *TAG = "MQTT";

// static uint32_t last_updated_jwt = 0;

/**
 * Function to initialise MQTT
 * 
 * Make sure NVS is already initialised
 */
void mqtt_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
}

// const uint8_t mqtt_google_pem_start[] =
const char *mqtt_google_pem_start =
    // this cert is the "minimal root CA" found: https://cloud.google.com/iot/docs/how-tos/mqtt-bridge
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBxTCCAWugAwIBAgINAfD3nVndblD3QnNxUDAKBggqhkjOPQQDAjBEMQswCQYD\n"
    "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8G\n"
    "A1UEAxMIR1RTIExUU1IwHhcNMTgxMTAxMDAwMDQyWhcNNDIxMTAxMDAwMDQyWjBE\n"
    "MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\n"
    "QzERMA8GA1UEAxMIR1RTIExUU1IwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATN\n"
    "8YyO2u+yCQoZdwAkUNv5c3dokfULfrA6QJgFV2XMuENtQZIG5HUOS6jFn8f0ySlV\n"
    "eORCxqFyjDJyRn86d+Iko0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUw\n"
    "AwEB/zAdBgNVHQ4EFgQUPv7/zFLrvzQ+PfNA0OQlsV+4u1IwCgYIKoZIzj0EAwID\n"
    "SAAwRQIhAPKuf/VtBHqGw3TUwUIq7TfaExp3bH7bjCBmVXJupT9FAiBr0SmCtsuk\n"
    "miGgpajjf/gFigGM34F9021bCWs1MbL0SA==\n"
    "-----END CERTIFICATE-----\n";

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    // const char *sub_topic_command = "/devices/{device-id}/commands/#";
    const char *sub_topic_config = "/devices/new-test-device/config";

    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        gpio_set_level(CONFIG_MQTT_LED_PIN, 1);

        msg_id = esp_mqtt_client_subscribe(client, sub_topic_config, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

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

    // const uint32_t update_interval = 3500; // seconds
    // const uint32_t last_known_unix = 1597852811;

    time_t now;
    time(&now);

    // while (now < last_known_unix)
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

    // if ((last_updated + ((float)jwt_exp - (0.1 * jwt_exp))) < now)
    // {
    //     printf("the value: %f   now:%d\n", (last_updated + ((float)jwt_exp - (0.1 * jwt_exp))), (uint32_t)now);
    //     ESP_LOGI(TAG, "need to update JWT");
    //     last_updated = now;
    //     return true;
    // }

    return false;
}