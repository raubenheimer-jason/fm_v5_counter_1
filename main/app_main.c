// FM_V5_counter_1

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"

#include "main.h"

#include "freertos/queue.h"

// Time
#include "components/time_init/time_init.h"

// FRAM
#include "components/fram/fram.h"

// wifi
#include "components/wifi/wifi.h"

// NTP
#include "components/ntp/ntp.h"

// GPIO
#include "components/gpio/gpio.h"

// MQTT
#include "components/mqtt/mqtt.h"

// status
#include "components/status/status.h"

// // For printing uint64_t
// #define __STDC_FORMAT_MACROS
// #include <inttypes.h>
// uint64_t i;
// printf("\t\tt.addr: %" PRIu64 "\n", t.addr);

static const char *TAG = "APP_MAIN";

// Semaphore for rtc_alarm_flag variable
// SemaphoreHandle_t rtc_alarm_flag_gatekeeper = 0; // binary type because of ISR
xSemaphoreHandle rtc_alarm_flag_gatekeeper = NULL;
xSemaphoreHandle status_struct_gatekeeper = NULL;

xQueueHandle fram_store_queue = NULL;
static xQueueHandle upload_queue = NULL;
static xQueueHandle ack_queue = NULL;

// bool rtc_alarm_flag = false;

bool restart_required_flag = false;

// ====================================================== MQTT

char device_id[20];

const char *private_key = CONFIG_DEVICE_PRIVATE_KEY;

extern const uint8_t mqtt_google_primary_pem[] asm("_binary_mqtt_google_primary_pem_start");
extern const uint8_t mqtt_google_backup_pem[] asm("_binary_mqtt_google_backup_pem_start");

// extern const char device_private_key[] asm("_binary_device_private_key_pem_start");

// ------------------------------------- END MQTT

// ====================================================== STATUS

uint8_t minute_count = 61; // Force status update on restart. Count of the minutes to know when an hour has passed (for uploading the status)
/* on_mains_flag
    1  = device on mains power
    0  = device running on battery power
    -1 = error reading the pin
*/
int8_t on_mains_flag; // default on battery power to start??

// ------------------------------------- END STATUS

void set_restart_required_flag()
{
    restart_required_flag = true;
    ESP_LOGW(TAG, "restart_required_flag set true");
}

// ================================================================================================= FRAM TASK

void Fram_Task_Code(void *pvParameters)
{
    // int8_t received_telemetry = 0; // 0 = no telemetry received, 1 = received telemetry

    TickType_t fram_store_ticks = 0; // dont wait first time, there might be a backlog and the chance of a new message in the queue right on startup is unlikely

    // int8_t have_backlog_flag = 0; // 1 = there is a backlog, 0 = there is no backlog

    for (;;)
    {

        // if (rtc_alarm_flag == true)
        // {
        //     if (xSemaphoreTake(rtc_alarm_flag_gatekeeper, 100) == pdTRUE)
        //     {
        //         rtc_alarm_flag = false;
        //         xSemaphoreGive(rtc_alarm_flag_gatekeeper);
        //         ESP_LOGI(TAG, "-------------------------- rtc alarm!! -------------------------- ");
        //         rtc_clear_alarm();
        //     }
        //     minute_count++;
        //     ESP_LOGD(TAG, "minutes passed: %d", minute_count);
        // }

        uint64_t telemetry_to_store = 0;
        // if (xQueueReceive(fram_store_queue, &telemetry_to_store, 0))
        if (xQueueReceive(fram_store_queue, &telemetry_to_store, fram_store_ticks))
        {
            ESP_LOGI(TAG, "-------------------------- rtc alarm!! -------------------------- ");
            ESP_LOGD(TAG, "received telemetry_to_store");

            // Store telemetry in FRAM
            if (write_telemetry(telemetry_to_store) == false)
            {
                ESP_LOGE(TAG, "error writing telemetry");
                if (xSemaphoreTake(status_struct_gatekeeper, 100) == pdTRUE)
                {
                    status_incrementFramWriteErrors();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
            }
            else
            {
                ESP_LOGD(TAG, "telemetry successfully stored in FRAM");
                if (xSemaphoreTake(status_struct_gatekeeper, 100) == pdTRUE)
                {
                    status_framHighWaterMark(get_stored_messages_count());
                    xSemaphoreGive(status_struct_gatekeeper);
                }
            }

            rtc_clear_alarm();
            minute_count++;
            ESP_LOGD(TAG, "minutes passed: %d", minute_count);
        }

        uint64_t telemetry_to_delete = 0;
        if (xQueueReceive(ack_queue, &telemetry_to_delete, 0))
        // if (xQueueReceive(ack_queue, &telemetry_to_delete, 500 / portTICK_PERIOD_MS))
        {
            if (telemetry_to_delete > CONFIG_LAST_KNOWN_UNIX)
            {
                // Delete telemetry from FRAM
                if (delete_last_read_telemetry(telemetry_to_delete) == true)
                {
                    ESP_LOGD(TAG, "telemetry successfully deleted from FRAM");
                }
            }
        }

        uint64_t telemetry_to_upload = read_telemetry();

        if (telemetry_to_upload > 0)
        {
            ESP_LOGD(TAG, "send telemetry from Fram_Task to Upload_Task");

            uint32_t unix_time = telemetry_to_upload >> 32;

            if (unix_time > CONFIG_LAST_KNOWN_UNIX)
            {
                if (xQueueSend(upload_queue, &telemetry_to_upload, 0))
                {
                    ESP_LOGD(TAG, "telemetry sent to Upload_Task");
                }
                else
                {
                    ESP_LOGD(TAG, "upload_queue must be full");
                }
            }
            else
            {
                ESP_LOGE(TAG, "error in data read from fram");
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementFramReadErrors();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
            }
            printf("-- short ticks to wait --\n");
            fram_store_ticks = 100 / portTICK_PERIOD_MS; // there might be a backlog so dont wait for new telemetry for long
        }
        else
        {
            printf("------- long ticks to wait -------\n");
            fram_store_ticks = 10000 / portTICK_PERIOD_MS; // no message backlog in FRAM, just wait for new message
        }

        // else
        // {
        //     vTaskDelay(200 / portTICK_PERIOD_MS);
        // }

        // vTaskDelay(1000 / portTICK_PERIOD_MS); // was 5000
        // vTaskDelay(200 / portTICK_PERIOD_MS); // was 5000
    }
}

// ================================================================================================= FRAM TASK

void mains_flag_evaluation(void)
{
    // Check if we are on mains or battery power.
    // Do this before WiFi and MQTT so the LED's don't turn on if device is on battery
    int8_t on_mains = status_onMains();
    if (on_mains_flag != on_mains)
    {
        if (on_mains == 0) // Not on mains (on battery)
        {
            ESP_LOGW(TAG, "device on battery power");
            on_mains_flag = 0;
            minute_count = 61; // force the status update
        }
        else if (on_mains == 1)
        {
            ESP_LOGI(TAG, "device on mains power");
            on_mains_flag = 1;
            minute_count = 61; // force the status update
        }
        else
        {
            ESP_LOGE(TAG, "weird error - is the device on battery or on mains power?");
            minute_count = 61; // force the status update
        }
    }
}

// void start_mqtt_client(const char *jwt, char *client_id, int8_t use_backup_certificate)
// {
//     char *certificate_in_use;
//     if (use_backup_certificate == 0)
//     {
//         ESP_LOGI(TAG, "use primary certificate");
//         certificate_in_use = mqtt_google_primary_pem;
//     }
//     else if (use_backup_certificate == 1)
//     {
//         ESP_LOGI(TAG, "use backup certificate");
//         certificate_in_use = mqtt_google_backup_pem;
//     }
//     else
//     {
//         ESP_LOGE(TAG, "what certificate are we meant to use?? just use primary");
//         certificate_in_use = mqtt_google_primary_pem;
//     }

//     esp_mqtt_client_config_t mqtt_cfg = {
//         .uri = CONFIG_GCP_URI,   // "mqtts://mqtt.2030.ltsapis.goog:8883"
//         .host = CONFIG_GCP_HOST, // "mqtt.2030.ltsapis.goog"
//         .port = CONFIG_GCP_PORT, // 8883
//         .username = "unused",
//         .password = jwt,
//         .client_id = client_id, // "projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device"
//         // .cert_pem = (const char *)mqtt_google_primary_pem,
//         // .cert_pem = (const char *)mqtt_google_backup_pem,
//         .cert_pem = (const char *)certificate_in_use,
//         .lwt_qos = 1};

//     ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
//     esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
//     esp_err_t register_ret = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
//     printf("register_ret: %d\n", register_ret);
//     // esp_mqtt_client_stop(client); // might prevent errors where the client is connected and there is a restart?????
//     esp_err_t disc_ret = esp_mqtt_client_disconnect(client); // might prevent errors where the client is connected and there is a restart?????
//     printf("disc_ret: %d\n", disc_ret);

//     esp_err_t start_ret = esp_mqtt_client_start(client);
//     printf("start_ret: %d\n", start_ret);
// }

// ================================================================================================= UPLOAD TASK

void Upload_Task_Code(void *pvParameters)
{
    // ============ MQTT ============

    jwt_update_check();

    time_t now;
    time(&now);

    char *jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
    // char *jwt = createJwt(device_private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS

    printf("jwt: %s\n", jwt);

    // Check for error allocating memory
    // Won't need to free this as it's used throughout the life of the program
    char *client_id = (char *)malloc(strlen("projects/") + strlen(CONFIG_GCP_PROJECT_ID) + strlen("/locations/") + strlen(CONFIG_GCP_LOCATION) + strlen("/registries/") + strlen(CONFIG_GCP_REGISTRY) + strlen("/devices/") + strlen(device_id) + 1);
    client_id[0] = '\0';
    strcat(client_id, "projects/"); // projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device
    strcat(client_id, CONFIG_GCP_PROJECT_ID);
    strcat(client_id, "/locations/");
    strcat(client_id, CONFIG_GCP_LOCATION);
    strcat(client_id, "/registries/");
    strcat(client_id, CONFIG_GCP_REGISTRY);
    strcat(client_id, "/devices/");
    strcat(client_id, device_id);
    printf("len = %d, client_id: %s\n", strlen(client_id), client_id);

    // int8_t use_backup_certificate = 0;
    // int8_t certificate_in_use = 1; // 1 = primary, 2 = backup
    // start_mqtt_client(jwt, client_id, use_backup_certificate);

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_GCP_URI,   // "mqtts://mqtt.2030.ltsapis.goog:8883"
        .host = CONFIG_GCP_HOST, // "mqtt.2030.ltsapis.goog"
        .port = CONFIG_GCP_PORT, // 8883
        .username = "unused",
        .password = jwt,
        .client_id = client_id, // "projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device"
        .cert_pem = (const char *)mqtt_google_primary_pem,
        // .cert_pem = (const char *)mqtt_google_backup_pem,
        .lwt_qos = 1};

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_err_t register_ret = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    printf("register_ret: %d\n", register_ret);
    // esp_mqtt_client_stop(client); // might prevent errors where the client is connected and there is a restart?????
    esp_err_t disc_ret = esp_mqtt_client_disconnect(client); // might prevent errors where the client is connected and there is a restart?????
    printf("disc_ret: %d\n", disc_ret);

    esp_err_t start_ret = esp_mqtt_client_start(client);
    printf("start_ret: %d\n", start_ret);

    // Won't need to free this as it's used throughout the life of the program
    char *telemetry_topic = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/events") + 1); // Check for error allocating memory
    telemetry_topic[0] = '\0';
    strcat(telemetry_topic, "/devices/"); // "/devices/new-test-device/events"
    strcat(telemetry_topic, device_id);
    strcat(telemetry_topic, "/events");
    printf("len = %d, telemetry_topic: %s\n", strlen(telemetry_topic), telemetry_topic);

    // ============ MQTT ============

    bool need_to_upload_flag = false;
    uint64_t previously_uploaded_telemetry = 0;
    uint64_t telemetry_to_upload = 0;

    uint32_t success_count = 0;
    uint32_t error_count = 0;

    // int8_t enter_deep_sleep_flag = 0;         // flag which gets triggered when running on battery power
    // int8_t software_update_flag = 0;          // flag to indicate a software update is avaliable
    // int8_t telemetry_upload_pending_flag = 0; // flag to indicate that there is still telemetry pending

    // on_mains_flag = -1;

    mqtt_connected_flag = 0; // 1 = connected, 0 = not connected

    for (;;)
    {
        // if (use_backup_certificate == 0 && certificate_in_use == 2)
        // {
        //     start_mqtt_client(jwt, client_id, 0);
        // }
        // else if (use_backup_certificate == 1 && certificate_in_use == 1)
        // {
        //     start_mqtt_client(jwt, client_id, 1);
        // }
        // // Check if we are on mains or battery power.
        // // Do this before WiFi and MQTT so the LED's don't turn on if device is on battery
        // int8_t on_mains = status_onMains();
        // if (on_mains_flag != on_mains)
        // {
        //     if (on_mains == 0) // Not on mains (on battery)
        //     {
        //         ESP_LOGW(TAG, "device on battery power");
        //         on_mains_flag = 0;
        //         minute_count = 61; // force the status update
        //     }
        //     else if (on_mains == 1)
        //     {
        //         ESP_LOGI(TAG, "device on mains power");
        //         on_mains_flag = 1;
        //         minute_count = 61; // force the status update
        //     }
        //     else
        //     {
        //         ESP_LOGI(TAG, "weird error - is the device on battery or on mains power?");
        //         minute_count = 61; // force the status update
        //     }
        // }
        // if ((on_mains == 1 && on_mains_flag == 0) || (on_mains == 0 && on_mains_flag == 1)) // on_mains == mains power, on_mains_flag == battery power
        // {
        //     ESP_LOGI(TAG, "on_mains_flag: %d, status_onMains(): %d", on_mains_flag, on_mains);
        //     on_mains_flag = !on_mains_flag; // update the on_mains_flag
        //     minute_count = 61;              // force the status update
        //     ESP_LOGI(TAG, "new on_mains_flag: %d", on_mains_flag);
        // }

        mains_flag_evaluation();

        // printf("waiting for message to be added to queue...\n");
        // uint64_t dummy_buf;
        // xQueuePeek(upload_queue, &dummy_buf, portMAX_DELAY);
        // printf("!!! message added to queue\n");

        while (mqtt_connected_flag == 0)
        {
            ESP_LOGW(TAG, "waiting for mqtt to connect...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        esp_err_t wifi_info_res = ESP_ERR_WIFI_NOT_CONNECT;
        do
        {
            wifi_ap_record_t ap_info;
            wifi_info_res = esp_wifi_sta_get_ap_info(&ap_info);

            if (wifi_info_res == ESP_ERR_WIFI_NOT_CONNECT)
            {
                ESP_LOGE(TAG, "WiFi not connected (ESP_ERR_WIFI_NOT_CONNECT)");
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementWifiDisconnections();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
            else
            {
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_setRssiLowWaterMark(ap_info.rssi);
                    xSemaphoreGive(status_struct_gatekeeper);
                }
            }

        } while (wifi_info_res == ESP_ERR_WIFI_NOT_CONNECT);

        // ============ MQTT ============

        if (jwt_update_check())
        {
            free(jwt); // USE REALLOC RATHER ????????????????????????????????????
            time(&now);
            jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
            // jwt = createJwt(device_private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
            ESP_LOGI(TAG, "updated JWT, now: %d", (uint32_t)now);
            esp_err_t stop_ret = esp_mqtt_client_stop(client);
            if (stop_ret == ESP_OK)
            {
                ESP_LOGI(TAG, "mqtt stop successful");

                mqtt_cfg.password = jwt;
                esp_err_t ret = esp_mqtt_set_config(client, &mqtt_cfg);
                if (ret == ESP_OK)
                {
                    ESP_LOGI(TAG, "JWT updated successfully, jwt: %s", jwt);
                }
                else
                {
                    ESP_LOGE(TAG, "unable to update mqtt client with new JWT");
                }

                esp_err_t start_ret = esp_mqtt_client_start(client);
                if (start_ret == ESP_OK)
                {
                    ESP_LOGI(TAG, "mqtt start successful");
                }
                else
                {
                    ESP_LOGE(TAG, "mqtt start fail");
                }
            }
            else
            {
                ESP_LOGE(TAG, "mqtt stop fail");
            }
        }
        // ============ MQTT END ============

        if (need_to_upload_flag == false && xQueueReceive(upload_queue, &telemetry_to_upload, 0))
        {
            if (telemetry_to_upload == previously_uploaded_telemetry)
            {
                uint32_t unix_time = telemetry_to_upload >> 32;
                uint32_t count = (uint32_t)telemetry_to_upload;

                // uint64_t i;
                // printf("\t\tt.addr: %" PRIu64 "\n", t.addr);
                ESP_LOGD(TAG, "telemetry_to_upload == previously_uploaded_telemetry (unix: %d, count: %d)", unix_time, count);
                if (xQueueSend(ack_queue, &telemetry_to_upload, 0))
                {
                    ESP_LOGD(TAG, "ack sent to Fram_Task");
                }
                else
                {
                    ESP_LOGD(TAG, "ack_queue must be full");
                }
            }
            else // just carry on as normal
            {
                need_to_upload_flag = true;
                // telemetry_upload_pending_flag = 1; // to prevent deep sleep (on battery) if there is telemetry pending
            }
        }
        // else
        // {
        //     telemetry_upload_pending_flag = 0;
        // }

        if (telemetry_to_upload > 0 && need_to_upload_flag == true) // keep trying to send the message until successful. If we dont have this, and the send fails, our queue will be empty but the FRAM task wont send anything else because we havent sent an ack
        {

            uint32_t unix_time = telemetry_to_upload >> 32;
            uint32_t count = (uint32_t)telemetry_to_upload;

            // Send telemetry to GCP here

            char telemetry_buf[40]; // {"t":1596255206,"v":4294967295} --> 31 characters
            snprintf(telemetry_buf, 40, "{\"t\":%d,\"v\":%d}", (int)unix_time, (int)count);

            printf("%s\n", telemetry_buf);

            // vTaskDelay(5000 / portTICK_PERIOD_MS);
            // ============ MQTT ============ START

            int32_t upload_res = esp_mqtt_client_publish(client, telemetry_topic, telemetry_buf, 0, 1, 0);

            // ============ MQTT ============ END

            // printf("upload_res: %d\n", upload_res);

            if (upload_res == 0)
            {
                printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! UPLOAD_RES == 0\n");
            }

            if (upload_res > 0) // surely message ID won't be 0 ??
            {
                success_count++;
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementMqttUploadSuccess();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
                ESP_LOGI(TAG, "-->> PUBLISH SUCCESS!!!!");

                previously_uploaded_telemetry = telemetry_to_upload;
                need_to_upload_flag = false;

                // if sending to GCP was successful
                //? what happens if sending to GCP fails? / now we have removed the value from the queue...
                //? it's still in the variable though so just make sure to upload it... (maybe have upload_success_flag ?)
                //? if the power is lost after the telemetry is received from the queue, that's fine, the ack wouldnt have been sent to FRAM if the data hasn't been uploaded to GCP
                //?   --> next time the device turns on, FRAM will send the same message back to UPLOAD because it didnt delete it because it didnt receive the ack
                // Send ack to FRAM (the unix that was uploaded to GCP)
                if (!xQueueSend(ack_queue, &telemetry_to_upload, 1000 / portTICK_PERIOD_MS)) //? dont think we need to wait here, if the queue is full there is a biggere problem...
                {
                    ESP_LOGE(TAG, "adding to ackQueue failed !!! ");
                }
            }
            else
            {
                error_count++;
                ESP_LOGE(TAG, "UPLOAD SENDING FAILED !!!");
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementMqttUploadErrors();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }

            uint32_t total = success_count + error_count;
            float success = ((float)success_count / total) * 100;
            printf("success: %.2f%%  (%d/%d)  (success = %d, error = %d, total = %d)\n", success, success_count, total, success_count, error_count, total);
        }
        // else
        // {
        //     // if there is no telemetry to upload delay and look again after some time (give the other tasks CPU time)
        //     // vTaskDelay(1000 / portTICK_PERIOD_MS);
        //     vTaskDelay(200 / portTICK_PERIOD_MS);
        // }

        // ================================= STATUS Upload stuff =================================

        // Check if we are on mains or battery (and if there was a change from battery to mains)
        // int8_t on_mains = status_onMains();
        // if ((on_mains == 1 && on_mains_flag == 0) || (on_mains == 0 && on_mains_flag == 1)) // on_mains == mains power, on_mains_flag == battery power
        // {
        //     ESP_LOGI(TAG, "on_mains_flag: %d, status_onMains(): %d", on_mains_flag, on_mains);
        //     on_mains_flag = !on_mains_flag; // update the on_mains_flag
        //     minute_count = 61;              // force the status update
        //     ESP_LOGI(TAG, "new on_mains_flag: %d", on_mains_flag);
        // }
        // else if (on_mains == 0 && on_mains_flag == 1)
        // {
        //     on_mains_flag = 0; // update the on_mains_flag
        //     minute_count = 61; // force the status update
        //     // enter_deep_sleep_flag = 1;
        // }

        if (minute_count > CONFIG_STATUS_UPLOAD_INTERVAL_MIN)
        {
            ESP_LOGI(TAG, "***------------------------------------ UPLOAD STATUS ------------------------------------***");

            // Evaluate latest power status before upload
            if (xSemaphoreTake(status_struct_gatekeeper, 100))
            {
                status_evaluatePower();
                xSemaphoreGive(status_struct_gatekeeper);
            }

            char status_message[350];
            // status_message[0] = '\0';

            // UPLOAD STATUS HERE
            int32_t upload_res = 0;
            if (xSemaphoreTake(status_struct_gatekeeper, 100))
            {
                status_printStatusStruct();
                get_status_message_json(status_message);

                xSemaphoreGive(status_struct_gatekeeper);
            }
            printf("status message:\n");
            printf("%s\n", status_message);

            char *status_telemetry_topic = (char *)malloc(strlen(telemetry_topic) + strlen(CONFIG_STATUS_SUBFOLDER) + 1 + 1); // + 1 for "/", + 1 for '\0'
            strcpy(status_telemetry_topic, telemetry_topic);
            strcat(status_telemetry_topic, "/");
            strcat(status_telemetry_topic, CONFIG_STATUS_SUBFOLDER);
            printf("status_telemetry_topic: %s\n", status_telemetry_topic);
            upload_res = esp_mqtt_client_publish(client, status_telemetry_topic, status_message, 0, 1, 0); // status-telemetry-1

            // IF upload was successful
            if (upload_res > 0)
            {
                ESP_LOGI(TAG, "**************** STATUS PUBLISH SUCCESS!!!! ****************");
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    // reset status struct
                    status_resetStruct();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
                minute_count = 1; // (or is it 0?? --> make sure to initialise the variable to the correct one)
            }
            else
            {
                ESP_LOGE(TAG, "upload STATUS sending failed  !!!");

                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
        }

        printf("waiting for message to be added to queue...\n");
        uint64_t dummy_buf;
        xQueuePeek(upload_queue, &dummy_buf, portMAX_DELAY);
        printf("!!! message added to queue\n");

        // if (enter_deep_sleep_flag == 1 && software_update == 0 && telemetry_upload_pending == 0)
        // {
        //     // enter deep sleep
        // }
    }
}

// ================================================================================================= UPLOAD TASK

static void start_upload_task(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t Upload_Task = NULL;
    const uint32_t STACK_SIZE = 24000;
    // const uint8_t task_priority = tskIDLE_PRIORITY;
    const uint8_t task_priority = 10;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate(Upload_Task_Code, "Upload_Task", STACK_SIZE, &ucParameterToPass, task_priority, &Upload_Task);
    configASSERT(Upload_Task);
}

static void start_fram_task()
{
    static uint8_t ucParameterToPass;
    TaskHandle_t Fram_Task = NULL;
    const uint32_t STACK_SIZE = 8000;
    const uint8_t task_priority = 9;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate(Fram_Task_Code, "Fram_Task", STACK_SIZE, &ucParameterToPass, task_priority, &Fram_Task);
    configASSERT(Fram_Task);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    // esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_log_level_set("APP_MAIN", ESP_LOG_VERBOSE);

    // FRAM
    fram_spi_init();

    // create queues
    fram_store_queue = xQueueCreate(10, sizeof(uint64_t));
    upload_queue = xQueueCreate(1, sizeof(uint64_t));
    ack_queue = xQueueCreate(1, sizeof(uint64_t));

    // Check queues
    bool need_to_restart = false;
    if (fram_store_queue == NULL)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "fram_store_queue == NULL");
    }
    if (upload_queue == NULL)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "upload_queue == NULL");
    }
    if (ack_queue == NULL)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "ack_queue == NULL");
    }

    // Semaphore for rtc_alarm_flag
    // rtc_alarm_flag_gatekeeper = xSemaphoreCreateBinary();
    rtc_alarm_flag_gatekeeper = xSemaphoreCreateMutex();
    status_struct_gatekeeper = xSemaphoreCreateMutex();

    if (rtc_alarm_flag_gatekeeper == NULL)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "rtc_alarm_flag_gatekeeper == NULL");
    }
    if (status_struct_gatekeeper == NULL)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "status_struct_gatekeeper == NULL");
    }

    // GPIO
    esp_err_t gpio_setup_ret = gpio_initial_setup();
    if (gpio_setup_ret != ESP_OK)
    {
        need_to_restart = true;
        ESP_LOGE(TAG, "gpio_setup_ret != ESP_OK");
    }

    if (need_to_restart == true)
    {
        ESP_LOGE(TAG, "restarting in 10s");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    on_mains_flag = -1; // force the evaluation
    mains_flag_evaluation();

    // Initial LED states
    if (on_mains_flag == 1) // only turn the LED on if device is on mains power
    {
        // printf("turning led on ---------------------------------------------------------------------------------------------------------------------------------------\n");
        gpio_set_level(CONFIG_WIFI_LED_PIN, 1);
    }
    gpio_set_level(CONFIG_MQTT_LED_PIN, 0);

    // status_evaluatePower();

    // status_setBatteryChargeStatus(true);
    // status_setOnMains(true);
    // status_setBatteryVoltage(1234);

    // status_printStatusStruct();

    // Check if we are on battery or mains

    // If mains, set that in status, carry on as normal

    // If battery, set that in status, collect other necessary data (such as battery level), upload that, sleep

    // Time
    time_init();

    // WiFi
    wifi_init_sta();

    // NTP
    initialize_sntp();

    // MQTT
    mqtt_init(); // make sure NVS is initiated first (done in wifi)

    // Tasks
    start_fram_task();
    start_upload_task();
}