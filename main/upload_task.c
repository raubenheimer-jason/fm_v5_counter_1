#include "upload_task.h"

// start heap tracing

// #include "esp_heap_trace.h"

// end heap tracing

static const char *TAG = "UPLOAD_TASK";

uint8_t minute_count = 61; // Force status update on restart. Count of the minutes to know when an hour has passed (for uploading the status)

char device_id[20];

const char *private_key = CONFIG_DEVICE_PRIVATE_KEY;

extern const uint8_t mqtt_primary_backup_pem[] asm("_binary_mqtt_primary_backup_pem_start"); // contains both primary and backup certificates (primary first, but doesn't seem to matter)

const uint32_t max_upload_errors = 60; // maximum upload errors before device resets
uint32_t upload_error_count = 0;       // if the number of errors exceeds a limit, restart device

int8_t mqtt_connected_flag = 0; // 1 = connected, 0 = not connected

/**
 * Only call function from UPLOAD TASK
 * Used before the restart (if restar_required_flag = true)
 */
// void stop_wifi_and_mqtt(esp_mqtt_client_handle_t client)
void stop_mqtt(esp_mqtt_client_handle_t client)
{
    ESP_LOGW(TAG, "disconnecting mqtt");
    esp_mqtt_client_disconnect(client);

    ESP_LOGW(TAG, "stopping mqtt");
    esp_mqtt_client_stop(client);

    ESP_LOGW(TAG, "disconnecting wifi");
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
}

/**
 * Increments the upload error count (only) if telemetry publish failed
 * Checks if the max errors has been exceeded, if it has, starts the restart process
 */
void increment_upload_error_count(esp_mqtt_client_handle_t client)
{
    upload_error_count++;
    ESP_LOGW(TAG, "upload_error_count++");
    if (upload_error_count > max_upload_errors)
    {
        ESP_LOGE(TAG, "upload_error_count > max_upload_errors, will begin the restart process");
        stop_mqtt(client);
        ESP_LOGI(TAG, "UPLOAD TASK shutdown complete, waiting for FRAM TASK");
        restart_required_flag = true;
        uint32_t count = 0;
        const uint32_t max_count = 100;
        for (;;)
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            if (count > max_count)
            {
                ESP_LOGE(TAG, "timeout exceeded waiting for restart_required_flag to restart device, forcing restart...");
                esp_restart();
            }
            count++;
        }
    }
}

/**
 * Decrements the upload_error_count if it is greater than 0
 * Only for telemetry publish success
 */
void decrement_upload_error_count(void)
{
    if (upload_error_count > 0)
    {
        upload_error_count--;
    }
}

/**
 * Make sure memory was allocated for the pointer
 */
void char_malloc_checker(const char *ptr, const char *ptr_name)
{
    if (ptr == NULL)
    {
        ESP_LOGE(TAG, "%s == NULL, setting restart_required_flag = true", ptr_name);
        restart_required_flag = true;
        uint8_t count = 0;
        uint8_t max_count = 100;
        for (;;)
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            if (count > max_count)
            {
                ESP_LOGE(TAG, "restart still hasn't occured, forcing restart...");
                esp_restart();
            }
            count++;
        }
    }
}

// ====================================================== MQTT

void Upload_Task_Code(void *pvParameters)
{
    // WiFi
    wifi_init_sta();

    // NTP
    initialize_sntp();

    // MQTT
    mqtt_init();

    // ============ MQTT ============

    jwt_update_check();

    time_t now;
    time(&now);

    // this is handled in jwt_update_check() <-- must have it before createJwt in case
    if (now < CONFIG_LAST_KNOWN_UNIX)
    {
        ESP_LOGW(TAG, "waiting for time to be updated before creating JWT... (time = %d)", (uint32_t)now);
        while (now < CONFIG_LAST_KNOWN_UNIX)
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            time(&now);
        }
        ESP_LOGI(TAG, "system time updated (now = %d)", (uint32_t)now);
    }

    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));

    char *jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS

    // ESP_ERROR_CHECK(heap_trace_stop());
    // heap_trace_dump();

    char_malloc_checker(jwt, "jwt");

    ESP_LOGI(TAG, "jwt: %s", jwt);

    // Check for error allocating memory
    // Won't need to free this as it's used throughout the life of the program
    // char *client_id = (char *)malloc(strlen("projects/") + strlen(CONFIG_GCP_PROJECT_ID) + strlen("/locations/") + strlen(CONFIG_GCP_LOCATION) + strlen("/registries/") + strlen(CONFIG_GCP_REGISTRY) + strlen("/devices/") + strlen(device_id) + 1);
    char *client_id = malloc(strlen("projects/") + strlen(CONFIG_GCP_PROJECT_ID) + strlen("/locations/") + strlen(CONFIG_GCP_LOCATION) + strlen("/registries/") + strlen(CONFIG_GCP_REGISTRY) + strlen("/devices/") + strlen(device_id) + 1);
    char_malloc_checker(client_id, "client_id");
    client_id[0] = '\0';
    strcat(client_id, "projects/"); // projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device
    strcat(client_id, CONFIG_GCP_PROJECT_ID);
    strcat(client_id, "/locations/");
    strcat(client_id, CONFIG_GCP_LOCATION);
    strcat(client_id, "/registries/");
    strcat(client_id, CONFIG_GCP_REGISTRY);
    strcat(client_id, "/devices/");
    strcat(client_id, device_id);
    ESP_LOGI(TAG, "len = %d, client_id: %s\n", strlen(client_id), client_id); // len = 91, client_id: projects/fm-development-1/locations/us-central1/registries/counter-1/devices/C-7CDFA1015100

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_GCP_URI,   // "mqtts://mqtt.2030.ltsapis.goog:8883"
        .host = CONFIG_GCP_HOST, // "mqtt.2030.ltsapis.goog"
        .port = CONFIG_GCP_PORT, // 8883
        .username = "unused",
        .password = jwt,
        .client_id = client_id,                            // "projects/fm-development-1/locations/us-central1/registries/counter-1/devices/new-test-device"
        .cert_pem = (const char *)mqtt_primary_backup_pem, // contains both primary and backup certificates
        .lwt_qos = 1};

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_err_t register_ret = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    ESP_LOGD(TAG, "register_ret: %d", register_ret);
    esp_err_t disc_ret = esp_mqtt_client_disconnect(client); // might prevent errors where the client is connected and there is a restart?????
    ESP_LOGD(TAG, "disc_ret: %d", disc_ret);
    esp_err_t start_ret = esp_mqtt_client_start(client);
    ESP_LOGD(TAG, "start_ret: %d", start_ret);

    // Won't need to free this as it's used throughout the life of the program
    // char *telemetry_topic = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/events") + 1); // Check for error allocating memory
    char *telemetry_topic = malloc(strlen("/devices/") + strlen(device_id) + strlen("/events") + 1); // Check for error allocating memory
    char_malloc_checker(telemetry_topic, "telemetry_topic");
    telemetry_topic[0] = '\0';
    strcat(telemetry_topic, "/devices/"); // "/devices/new-test-device/events"
    strcat(telemetry_topic, device_id);
    strcat(telemetry_topic, "/events");
    ESP_LOGI(TAG, "len = %d, telemetry_topic: %s", strlen(telemetry_topic), telemetry_topic); // len = 30, telemetry_topic: /devices/C-7CDFA1015100/events

    // Won't need to free this as it's used throughout the life of the program
    // char *status_telemetry_topic = (char *)malloc(strlen(telemetry_topic) + strlen(CONFIG_STATUS_SUBFOLDER) + 1 + 1); // + 1 for "/", + 1 for '\0'
    char *status_telemetry_topic = malloc(strlen(telemetry_topic) + strlen(CONFIG_STATUS_SUBFOLDER) + 1 + 1); // + 1 for "/", + 1 for '\0'
    char_malloc_checker(status_telemetry_topic, "status_telemetry_topic");
    strcpy(status_telemetry_topic, telemetry_topic);
    strcat(status_telemetry_topic, "/");
    strcat(status_telemetry_topic, CONFIG_STATUS_SUBFOLDER);
    ESP_LOGI(TAG, "status_telemetry_topic: %s", status_telemetry_topic); // status_telemetry_topic: /devices/C-7CDFA1015100/events/status-telemetry-1

    // Won't need to free this as it's used throughout the life of the program
    // char *state_topic = (char *)malloc(strlen("/devices/") + strlen(device_id) + strlen("/state") + 1); // Check for error allocating memory
    char *state_topic = malloc(strlen("/devices/") + strlen(device_id) + strlen("/state") + 1); // Check for error allocating memory
    char_malloc_checker(state_topic, "state_topic");
    state_topic[0] = '\0';
    strcpy(state_topic, "/devices/");
    strcat(state_topic, device_id);
    strcat(state_topic, "/state");
    ESP_LOGI(TAG, "state_topic: %s", state_topic); // state_topic: /devices/C-7CDFA1015100/state

    // ============ MQTT ============

    bool need_to_upload_flag = false;
    uint64_t previously_uploaded_telemetry = 0;
    uint64_t telemetry_to_upload = 0;

    uint32_t success_count = 0;
    uint32_t error_count = 0; // just for displaying to the console

    for (;;)
    {
        ESP_LOGD(TAG, "free heap start: %d", esp_get_free_heap_size());

        printf("[upload task] stack high water mark: %d\n", uxTaskGetStackHighWaterMark(NULL));

        // if (mqtt_connected_flag == 0)
        // {
        //     ESP_LOGW(TAG, "waiting for mqtt to connect...");
        //     while (mqtt_connected_flag == 0)
        //     {
        //         vTaskDelay(1000 / portTICK_PERIOD_MS);
        //     }
        //     ESP_LOGI(TAG, "mqtt connected!");
        // }

        bool mqtt_disconnected = false;
        esp_err_t wifi_info_res = ESP_ERR_WIFI_NOT_CONNECT;
        do
        {
            wifi_ap_record_t ap_info;
            wifi_info_res = esp_wifi_sta_get_ap_info(&ap_info);

            if (wifi_info_res == ESP_ERR_WIFI_NOT_CONNECT)
            {
                ESP_LOGE(TAG, "WiFi not connected (ESP_ERR_WIFI_NOT_CONNECT)");

                if (mqtt_disconnected == false)
                {
                    ESP_LOGW(TAG, "no wifi - disconnecting mqtt");
                    esp_mqtt_client_disconnect(client);

                    ESP_LOGW(TAG, "no wifi - stopping mqtt");
                    esp_mqtt_client_stop(client);
                    mqtt_disconnected = true;
                }

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

        if (mqtt_connected_flag == 0)
        {
            ESP_LOGW(TAG, "waiting for mqtt to connect...");
            while (mqtt_connected_flag == 0)
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "mqtt connected!");
        }

        // ============ MQTT ============

        if (jwt_update_check())
        {
            printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

            free(jwt); // Free the old jwt pointer
            time(&now);
            jwt = createJwt(private_key, CONFIG_GCP_PROJECT_ID, CONFIG_JWT_EXP, (uint32_t)now); // DONT FREE THIS
            char_malloc_checker(jwt, "jwt");
            ESP_LOGD(TAG, "updated JWT, now: %d", (uint32_t)now);
            esp_err_t stop_ret = esp_mqtt_client_stop(client);
            if (stop_ret == ESP_OK)
            {
                ESP_LOGD(TAG, "mqtt stop successful");

                mqtt_connected_flag = 0;

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
                    ESP_LOGD(TAG, "mqtt start successful");
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

            if (mqtt_connected_flag == 0)
            {
                printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

                ESP_LOGW(TAG, "waiting for mqtt to connect after jwt refresh...");
                while (mqtt_connected_flag == 0)
                {
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                ESP_LOGI(TAG, "mqtt connected!");
            }
            printf("fh: %d    min fh: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
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
            }
        }

        if (telemetry_to_upload > 0 && need_to_upload_flag == true) // keep trying to send the message until successful. If we dont have this, and the send fails, our queue will be empty but the FRAM task wont send anything else because we havent sent an ack
        {

            uint32_t unix_time = telemetry_to_upload >> 32;
            uint32_t count = (uint32_t)telemetry_to_upload;

            // Send telemetry to GCP here

            char telemetry_buf[40]; // {"t":1596255206,"v":4294967295} --> 31 characters
            snprintf(telemetry_buf, 40, "{\"t\":%d,\"v\":%d}", unix_time, count);

            ESP_LOGD(TAG, "%s", telemetry_buf);

            // ============ MQTT ============ START

            int32_t upload_res = esp_mqtt_client_publish(client, telemetry_topic, telemetry_buf, 0, 1, 0);

            // ============ MQTT ============ END

            if (upload_res > 0) // surely message ID won't be 0 ??
            {
                decrement_upload_error_count(); // increment_upload_error_count(client); // will set restart_required_flag = true if the count exceeds the limit
                success_count++;                // just for display
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementMqttUploadSuccess();
                    xSemaphoreGive(status_struct_gatekeeper);
                }

                uint32_t total = success_count + error_count;
                float success = ((float)success_count / total) * 100;
                ESP_LOGI(TAG, "%s   -->> PUBLISH SUCCESS!!!!  -- > success: %.2f%%  (%d/%d)  (success = %d, error = %d, total = %d) -- fh: %d  -- min fh: %d", telemetry_buf, success, success_count, total, success_count, error_count, total, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

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
                increment_upload_error_count(client); // will set restart_required_flag = true if the count exceeds the limit
                error_count++;                        // just for display
                uint32_t total = success_count + error_count;
                float success = ((float)success_count / total) * 100;
                ESP_LOGE(TAG, "%s    UPLOAD SENDING FAILED !!! -- > success: %.2f%%  (%d/%d)  (success = %d, error = %d, total = %d) -- fh: %d  -- min fh: %d  **********************", telemetry_buf, success, success_count, total, success_count, error_count, total, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
                if (xSemaphoreTake(status_struct_gatekeeper, 100))
                {
                    status_incrementMqttUploadErrors();
                    xSemaphoreGive(status_struct_gatekeeper);
                }
                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
        }

        // ================================= STATUS Upload stuff =================================

        if (minute_count > CONFIG_STATUS_UPLOAD_INTERVAL_MIN)
        {
            ESP_LOGD(TAG, "***------------------------------------ UPLOAD STATUS ------------------------------------***");

            // Evaluate latest power status before upload
            if (xSemaphoreTake(status_struct_gatekeeper, 100))
            {
                status_evaluatePower();
                xSemaphoreGive(status_struct_gatekeeper);
            }

            char status_message[350];

            // UPLOAD STATUS HERE
            int32_t upload_res = 0;
            if (xSemaphoreTake(status_struct_gatekeeper, 100))
            {
                get_status_message_json(status_message);
                xSemaphoreGive(status_struct_gatekeeper);
            }
            ESP_LOGI(TAG, "status message: %s", status_message);

            upload_res = esp_mqtt_client_publish(client, status_telemetry_topic, status_message, 0, 1, 0); // status-telemetry-1

            // IF upload was successful
            if (upload_res > 0)
            {
                ESP_LOGI(TAG, "STATUS PUBLISH SUCCESS!");
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
                ESP_LOGI(TAG, "status_telemetry_topic: %s", status_telemetry_topic); // status_telemetry_topic: /devices/C-7CDFA1015100/events/status-telemetry-1

                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }

            // ---------------- PUBLISH STATE (only first time microcontroller turns on) ----------------

            ESP_LOGD(TAG, "publishing device state...");

            int32_t state_upload_res = esp_mqtt_client_publish(client, state_topic, status_message, 0, 1, 0);

            if (state_upload_res > 0)
            {
                ESP_LOGI(TAG, "STATE PUBLISH SUCCESS!");
            }
            else
            {
                ESP_LOGE(TAG, "error publishing state, will try again next time device status gets published");
                ESP_LOGI(TAG, "state_topic: %s", state_topic); // state_topic: /devices/C-7CDFA1015100/state
            }
        }

        ESP_LOGD(TAG, "min fh: %d\n", esp_get_minimum_free_heap_size());

        uint64_t dummy_buf;
        xQueuePeek(upload_queue, &dummy_buf, 90000 / portTICK_PERIOD_MS); // like a delay but the delay will end if there is a new message
    }
}