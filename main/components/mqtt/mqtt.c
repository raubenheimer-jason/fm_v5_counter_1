#include "mqtt.h"

static char *getValueFromJson(const char *json_str, const uint32_t json_str_len, const char *key);
static void firmware_update_check(const char *config_data, const int config_data_len);
static esp_err_t firmware_update(const char *file_url, const char *certificate);
static bool is_json_valid(const char *json, uint32_t json_len);

char device_id[20];

/*  maximum number of times an mqtt error (or disconnection) can occur (consecutively) before restarting the device
    Error occurs then mqtt disconnects, so just increment in the error case
*/
static const uint32_t mqtt_max_error_count = 120;

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

    ESP_LOGI(TAG, "get_device_id res: %d, dev id: %s", res, device_id);

    // Done for wifi ??
    // ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "MQTT init done");
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    static uint32_t mqtt_error_count = 0;

    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

        mqtt_connected_flag = 1;

        if (on_mains_flag == 1) // only turn LED on if on mains power
        {
            gpio_set_level(CONFIG_MQTT_LED_PIN, 1);
        }

        // char *sub_topic_config = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/config") + 1); // Check for error allocating memory
        char *sub_topic_config = malloc(strlen("/devices/") + strlen(device_id) + strlen("/config") + 1); // Check for error allocating memory
        sub_topic_config[0] = '\0';
        strcpy(sub_topic_config, "/devices/");
        strcat(sub_topic_config, device_id);
        strcat(sub_topic_config, "/config");

        msg_id = esp_mqtt_client_subscribe(client, sub_topic_config, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        free(sub_topic_config);

        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected_flag = 0;
        gpio_set_level(CONFIG_MQTT_LED_PIN, 0);
        ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        mqtt_error_count = 0;   // reset the error count when publish success
        if (on_mains_flag == 1) // only turn LED on if on mains power
        {
            gpio_set_level(CONFIG_MQTT_LED_PIN, 1);
        }
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        ESP_LOGD(TAG, "DATA LEN=%d", event->data_len);

        if (is_json_valid(event->data, event->data_len) == true)
        {
            if (restart_required_flag == false)
            {
                firmware_update_check(event->data, event->data_len);
            }
        }
        else
        {
            ESP_LOGE(TAG, "config data does not contain valid json");
        }

        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

        break;
    case MQTT_EVENT_ERROR:
        mqtt_error_count++;
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR (mqtt_error_count = %d)", mqtt_error_count);
        if (mqtt_error_count > mqtt_max_error_count)
        {
            ESP_LOGE(TAG, "setting restart required flay = true (mqtt_max_error_count = %d)", mqtt_max_error_count);
            restart_required_flag = true;
        }

        if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);

            if (event->error_handle->esp_tls_stack_err == 0x2700)
            {
                ESP_LOGE(TAG, "CHANGE CERTIGICATES ????????????????????");
            }
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }

        ESP_LOGE(TAG, " - delaying 60s to not get rejected from server for retrying too many times");
        vTaskDelay(60000 / portTICK_PERIOD_MS);

        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
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
 * Basic check for if json is valid
 * (or at least all the characters are probably there)
 * 
 * @param json = pointer to the json string
 * @param json_len = length of json string (including \0)
 */
static bool is_json_valid(const char *json, uint32_t json_len)
{
    if (json_len < 1)
    {
        ESP_LOGE(TAG, "error with json (json_len < 1, json_len = %d)", json_len);
        return false;
    }

    if (json[0] != '{' || json[json_len - 1] != '}')
    {
        ESP_LOGE(TAG, "error with json (not starting and ending in \"{}\", got %c and %c instead)", json[0], json[json_len - 1]);
        return false;
    }

    ESP_LOGD(TAG, "valid json");
    return true;
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
        ESP_LOGW(TAG, "waiting for time to be updated before updating JWT (now = %ld)", now);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        time(&now);
    }

    if ((last_updated + ((float)CONFIG_JWT_EXP - (0.1 * CONFIG_JWT_EXP))) < now)
    {
        ESP_LOGW(TAG, "need to update JWT");
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

    sprintf(device_id, "C-%02X%02X%02X%02X%02X%02X", raw_mac[0], raw_mac[1], raw_mac[2], raw_mac[3], raw_mac[4], raw_mac[5]);

    return ESP_OK;
}

// ================================================================================== SOFTWARRE UPDATE

static void firmware_update_check(const char *config_data, const int config_data_len)
{
    const char *firmware_version_key = "firmware_version";
    const char *update_url_key = "url";

    ESP_LOGD(TAG, "checking firmware version for update");
    char *config_firmware_version = getValueFromJson(config_data, config_data_len, firmware_version_key);

    if (config_firmware_version != NULL)
    {
        if (strcmp(config_firmware_version, CONFIG_FIRMWARE_VERSION) != 0)
        {
            ESP_LOGI(TAG, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ FIRMWARE UPDATE AVALIABLE ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

            // Get url from config data (and certificate??)

            char *update_url = getValueFromJson(config_data, config_data_len, update_url_key);
            if (update_url != NULL)
            {
                ESP_LOGI(TAG, "got update url from config");
                printf("update_url: %s\n", update_url);

                /*  When the time comes to update the certificate (15 December 2021), we can have both
                    certificates here at the same time (one after the other in the same string, just
                    like the mqtt_primary_backup.pem certificates). 
                    We must just make sure to send an OTA update with the new certificate (as well as
                    the old one) before the old one expires. Thereafter we can send another OTA update
                    with only the new certificate (probably not necessary though). 
                */
                const char *update_certificate =
                    "-----BEGIN CERTIFICATE-----\n"
                    "MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4G\n"
                    "A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNp\n"
                    "Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1\n"
                    "MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEG\n"
                    "A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n"
                    "hvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8omUVCxKs+IVSbC9N/hHD6ErPL\n"
                    "v4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7SqbKSaZeqKeMWhG8\n"
                    "eoLrvozps6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQBoZfXklq\n"
                    "tTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzd\n"
                    "C9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pa\n"
                    "zq+r1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCB\n"
                    "mTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUm+IH\n"
                    "V2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5n\n"
                    "bG9iYWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG\n"
                    "3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4Gs\n"
                    "J0/WwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO\n"
                    "291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu01yiPqFbQfXf5WRDLenVOavS\n"
                    "ot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG79G+dwfCMNYxd\n"
                    "AfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7\n"
                    "TBj0/VLZjmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==\n"
                    "-----END CERTIFICATE-----\n";

                // download firmware update
                esp_err_t ret = firmware_update(update_url, update_certificate);

                free(update_url);

                // if firmware download was successful, set restart_required_flag = true
                if (ret == ESP_OK)
                {
                    // set restart_required_flag = true;
                    ESP_LOGW(TAG, "_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ SET RESTART REQUIRED FLAG = TRUE");
                    restart_required_flag = true;
                }
            }
            else
            {
                ESP_LOGW(TAG, "config doesn't contain update_url information...\n");
            }
        }
        else
        {
            ESP_LOGI(TAG, "no new firmware, device is up to date");
        }
    }
    else
    {
        ESP_LOGW(TAG, "config doesn't contain firmware_version information...\n");
    }

    free(config_firmware_version);
}

/** Returns a pointer to the value from the JSON
 *  
 * MUST FREE POINTER AFTER USE
 */
static char *getValueFromJson(const char *json_str, const uint32_t json_str_len, const char *key)
{
    // we don't know how big the value will be but it can't be bigger than this
    // char *value = (char *)malloc(json_str_len); // message base 64 ??
    char *value = malloc(json_str_len); // message base 64 ??
    value[0] = '\0';

    uint32_t s_index = 0;

    for (uint32_t i = 0; i < json_str_len; i++)
    {
        if (i < (json_str_len - strlen(key)))
        {
            // This section gets through the "key"
            bool done = true;
            for (uint32_t j = 0; j < strlen(key); j++)
            {
                if (key[j] == json_str[i + j])
                {
                    s_index = i + j;
                }
                else
                {
                    done = false;
                    break;
                }
            }
            // This section reads the value after the key has been found
            if (done == true)
            {
                uint32_t q_count = 0;
                for (uint32_t k = s_index + 1; k < json_str_len; k++)
                {
                    if (json_str[k] == '"')
                    {
                        q_count++;
                    }

                    if (q_count >= 2)
                    {
                        uint32_t count = 1;

                        // Read each character of the value until the last " (we do NOT cater for \")
                        while (json_str[k + count] != '"')
                        {
                            value[count - 1] = json_str[k + count];
                            value[count] = '\0'; // make surre to keep null value at end of string (dont put it after the count++)
                            count++;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (strlen(value) > 0)
    {
        // reallocate memory to just the size of the value
        value = (char *)realloc(value, strlen(value));
        return value;
    }
    else
    {
        free(value);
        return NULL;
    }
}

/**
 * Download new firmware.
 * 
 * Downloads the new firmware from the specified URL.
 * 
 * @param file_url: Url (including token) of the location of the firmware.bin file in cloud storage
 * @param certificate: Public certificate for cloud storage
 * 
 * @return esp_err_t: ESP_OK if download was successful, the error otherwise
 */

static esp_err_t firmware_update(const char *file_url, const char *certificate)
{
    ESP_LOGI(TAG, "downloading firmware update");

    esp_http_client_config_t config = {
        .url = file_url,
        .cert_pem = certificate,
    };

    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "successfully downloaded new firmware");
        printf("[upload task] stack high water mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "could not downloaded new firmware");
        return ret;
    }
}