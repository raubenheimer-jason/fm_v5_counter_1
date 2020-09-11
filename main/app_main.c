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

// // For printing uint64_t
// #define __STDC_FORMAT_MACROS
// #include <inttypes.h>
// uint64_t i;
// printf("\t\tt.addr: %" PRIu64 "\n", t.addr);

// const char *private_key asm("_binary_device_private_key_txt_start");

char device_id[20];

static const char *TAG = "APP_MAIN";

// Semaphore for rtc_alarm_flag variable
xSemaphoreHandle rtc_alarm_flag_gatekeeper = 0;

xQueueHandle fram_store_queue = NULL;
static xQueueHandle upload_queue = NULL;
static xQueueHandle ack_queue = NULL;

bool rtc_alarm_flag = false;

bool restart_required_flag = false;

void set_restart_required_flag()
{
    restart_required_flag = true;
    ESP_LOGW(TAG, "restart_required_flag set true");
}

// ================================================================================================= FRAM TASK

void Fram_Task_Code(void *pvParameters)
{
    for (;;)
    {

        if (rtc_alarm_flag == true)
        {
            if (xSemaphoreTake(rtc_alarm_flag_gatekeeper, 100))
            {
                rtc_alarm_flag = false;
                xSemaphoreGive(rtc_alarm_flag_gatekeeper);
                ESP_LOGI(TAG, "-------------------------- rtc alarm!! -------------------------- ");
                rtc_clear_alarm();
            }
        }

        uint64_t telemetry_to_store = 0;
        if (xQueueReceive(fram_store_queue, &telemetry_to_store, 0))
        {
            ESP_LOGI(TAG, "received telemetry_to_store");

            // Store telemetry in FRAM
            if (write_telemetry(telemetry_to_store) == false)
            {
                ESP_LOGE(TAG, "error writing telemetry");
            }
            else
            {
                ESP_LOGI(TAG, "telemetry successfully stored in FRAM");
            }
        }

        uint64_t telemetry_to_delete = 0;
        if (xQueueReceive(ack_queue, &telemetry_to_delete, 0))
        {
            if (telemetry_to_delete > CONFIG_LAST_KNOWN_UNIX)
            {
                // Store telemetry in FRAM
                if (delete_last_read_telemetry(telemetry_to_delete) == true)
                {
                    ESP_LOGI(TAG, "telemetry successfully deleted from FRAM");
                }
            }
        }

        uint64_t telemetry_to_upload = read_telemetry();

        if (telemetry_to_upload > 0)
        {
            ESP_LOGI(TAG, "send telemetry from Fram_Task to Upload_Task");

            uint32_t unix_time = telemetry_to_upload >> 32;

            // if (unix_time > last_known_unix)
            if (unix_time > CONFIG_LAST_KNOWN_UNIX)
            {
                if (xQueueSend(upload_queue, &telemetry_to_upload, 0))
                {
                    ESP_LOGI(TAG, "telemetry sent to Upload_Task");
                }
                else
                {
                    ESP_LOGI(TAG, "upload_queue must be full");
                }
            }
            else
            {
                ESP_LOGE(TAG, "error in data read from fram");
            }
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// ================================================================================================= FRAM TASK

// ================================================================================================= UPLOAD TASK

void Upload_Task_Code(void *pvParameters)
{
    // ============ MQTT ============

    jwt_update_check();

    time_t now;
    time(&now);

    // char *jwt = createJwt(private_key, "fm-development-1", CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
    char *jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
    // char *jwt = createJwt(CONFIG_DEVICE_PRIVATE_KEY, "fm-development-1", CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS

    printf("jwt: %s\n", jwt);

    // const char *jwt_const = (const char *)jwt;

    printf("******************************************************** dev id in main ___: %s\n", device_id);

    // char client_id[200] = "projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device";
    // char client_id[200] = "projects/" CONFIG_GCP_PROJECT_ID "/locations/" CONFIG_GCP_LOCATION "/registries/" CONFIG_GCP_REGISTRY "/devices/";

    // char client_id[200] = "projects/";        // fm-development-1/locations/us-central1/registries/counter-1/devices/"; // This works
    // strcat(client_id, CONFIG_GCP_PROJECT_ID); // projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device
    // strcat(client_id, "/locations/");
    // strcat(client_id, CONFIG_GCP_LOCATION);
    // strcat(client_id, "/registries/");
    // strcat(client_id, CONFIG_GCP_REGISTRY);
    // strcat(client_id, "/devices/");
    // strcat(client_id, device_id);

    // Check for error allocating memory
    // Won't need to free this as it's used throughout the life of the program
    char *client_id = (char *)malloc(strlen("projects/") + strlen(CONFIG_GCP_PROJECT_ID) + strlen("/locations/") + strlen(CONFIG_GCP_LOCATION) + strlen("/registries/") + strlen(CONFIG_GCP_REGISTRY) + strlen("/devices/") + strlen(device_id) + 1);
    client_id[0] = '\0';
    strcat(client_id, "projects/");           // projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device
    strcat(client_id, CONFIG_GCP_PROJECT_ID); // projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device
    strcat(client_id, "/locations/");
    strcat(client_id, CONFIG_GCP_LOCATION);
    strcat(client_id, "/registries/");
    strcat(client_id, CONFIG_GCP_REGISTRY);
    strcat(client_id, "/devices/");
    strcat(client_id, device_id);
    printf("len = %d, client_id: %s\n", strlen(client_id), client_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        // .uri = "mqtts://mqtt.2030.ltsapis.goog:8883",
        .uri = CONFIG_GCP_URI,
        // .host = "mqtt.2030.ltsapis.goog",
        .host = CONFIG_GCP_HOST,
        // .port = 8883,
        .port = CONFIG_GCP_PORT,
        .username = "unused",
        .password = jwt,
        // .client_id = "projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device",
        .client_id = client_id,
        .cert_pem = (const char *)mqtt_google_pem_start,
        .lwt_qos = 1};

    printf("pass: %s\n", mqtt_cfg.password);
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    // Won't need to free this as it's used throughout the life of the program
    char *telemetry_topic = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/events") + 1); // Check for error allocating memory
    telemetry_topic[0] = '\0';
    strcat(telemetry_topic, "/devices/");
    strcat(telemetry_topic, device_id);
    strcat(telemetry_topic, "/events");
    printf("len = %d, telemetry_topic: %s\n", strlen(telemetry_topic), telemetry_topic);

    // char telemetry_topic[60] = "/devices/"; // "/devices/new-test-device/events"
    // strcat(telemetry_topic, device_id);
    // strcat(telemetry_topic, "/events");

    // ============ MQTT ============

    bool need_to_upload_flag = false;
    uint64_t previously_uploaded_telemetry = 0;

    for (;;)
    {
        esp_err_t wifi_info_res = ESP_ERR_WIFI_NOT_CONNECT;
        do
        {
            wifi_ap_record_t ap_info;
            wifi_info_res = esp_wifi_sta_get_ap_info(&ap_info);

            if (wifi_info_res == ESP_ERR_WIFI_NOT_CONNECT)
            {
                ESP_LOGE(TAG, "WiFi not connected (ESP_ERR_WIFI_NOT_CONNECT)");
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        } while (wifi_info_res == ESP_ERR_WIFI_NOT_CONNECT);

        // ============ MQTT ============

        if (jwt_update_check())
        {
            free(jwt); // USE REALLOC RATHER ????????????????????????????????????
            time(&now);
            // jwt = createJwt(private_key, "fm-development-1", CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
            jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
            // jwt = createJwt(CONFIG_DEVICE_PRIVATE_KEY, "fm-development-1", CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
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

        uint64_t telemetry_to_upload = 0;
        if (need_to_upload_flag == false && xQueueReceive(upload_queue, &telemetry_to_upload, 0))
        {
            if (telemetry_to_upload == previously_uploaded_telemetry)
            {
                uint32_t unix_time = telemetry_to_upload >> 32;
                uint32_t count = (uint32_t)telemetry_to_upload;

                // uint64_t i;
                // printf("\t\tt.addr: %" PRIu64 "\n", t.addr);
                ESP_LOGI(TAG, "telemetry_to_upload == previously_uploaded_telemetry (unix: %d, count: %d)", unix_time, count);
                if (xQueueSend(ack_queue, &telemetry_to_upload, 0))
                {
                    ESP_LOGI(TAG, "ack sent to Fram_Task");
                }
                else
                {
                    ESP_LOGI(TAG, "ack_queue must be full");
                }
            }
            else // just carry on as normal
            {
                need_to_upload_flag = true;
            }
        }

        if (telemetry_to_upload > 0 && need_to_upload_flag == true) // keep trying to send the message until successful. If we dont have this, and the send fails, our queue will be empty but the FRAM task wont send anything else because we havent sent an ack
        {

            uint32_t unix_time = telemetry_to_upload >> 32;
            uint32_t count = (uint32_t)telemetry_to_upload;

            // Send telemetry to GCP here

            char telemetry_buf[40]; // {"t":1596255206,"v":4294967295} --> 31 characters
            snprintf(telemetry_buf, 40, "{\"t\":%d,\"v\":%d}", (int)unix_time, (int)count);

            printf("%s\n", telemetry_buf);

            vTaskDelay(5000 / portTICK_PERIOD_MS);
            // ============ MQTT ============ START

            // int32_t upload_res = esp_mqtt_client_publish(client, "/devices/new-test-device/events", telemetry_buf, 0, 1, 0);
            int32_t upload_res = esp_mqtt_client_publish(client, telemetry_topic, telemetry_buf, 0, 1, 0);

            // ============ MQTT ============ END

            // Just for now before we implement MQTT
            // bool upload_res = true;

            printf("upload_res: %d\n", upload_res);

            if (upload_res == 0)
            {
                printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! UPLOAD_RES == 0\n");
            }

            // if (upload_res == true)
            // if (upload_res != -1)
            if (upload_res > 0) // surely message ID won't be 0 ??
            {

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
                ESP_LOGE(TAG, "UPLOAD SENDING FAILED !!!");
                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
        }
        else
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
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

    // esp_err_t res = get_device_id(device_id);

    // printf("res: %d, dev id: %s\n", res, device_id);

    // for (;;)
    // {
    //     vTaskDelay(100);
    // }

    // FRAM
    fram_spi_init();

    // create queues
    fram_store_queue = xQueueCreate(10, sizeof(uint64_t));
    upload_queue = xQueueCreate(1, sizeof(uint64_t));
    ack_queue = xQueueCreate(1, sizeof(uint64_t));

    // ----------------------------------------------------     PUT THIS IN FOR THE QUEUES !!!!!!!!!!!!!!
    // if (telemetry_queue == NULL)
    // {
    //     ESP_LOGE(TAG, "Error creating the queue, restarting in 10s");
    //     vTaskDelay(10000 / portTICK_PERIOD_MS);
    //     esp_restart();
    // }

    // Semaphore for rtc_alarm_flag
    rtc_alarm_flag_gatekeeper = xSemaphoreCreateMutex();

    // GPIO
    gpio_initial_setup();

    // Tasks
    start_fram_task();
    start_upload_task();

    // Time
    time_init();

    // WiFi
    wifi_init_sta();

    // NTP
    initialize_sntp();

    // MQTT
    mqtt_init(); // make sure NVS is initiated first (done in wifi)
}