// FM_V5_counter_1

// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
/*
    -   -- DONE -- Have a safety that resets the device if the number of failed mqtt messages gets too high
                   (also count down if there are successful messages, similar to winery thermometer)
    -   test restart safety ^^

    -   -- DONE -- Put primary certificate before backup in pem file (shouldn't make a difference? -- at least test)

    -   Make OTA more robust

    -   Check if sensor could see reflector or not when it gets a 0 reaading

    -   Publish state to GCP IOT Core?

    -   Test flash encryption etc.
*/
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO

#include "main.h"

// // For printing uint64_t
// #define __STDC_FORMAT_MACROS
// #include <inttypes.h>
// uint64_t i;
// printf("\t\tt.addr: %" PRIu64 "\n", t.addr);

static const char *TAG = "APP_MAIN";

xSemaphoreHandle status_struct_gatekeeper = NULL;

xQueueHandle fram_store_queue = NULL;
// static xQueueHandle upload_queue = NULL;
// static xQueueHandle ack_queue = NULL;

xQueueHandle upload_queue = NULL;
xQueueHandle ack_queue = NULL;

bool restart_required_flag;

// // ====================================================== MQTT

// char device_id[20];

// const char *private_key = CONFIG_DEVICE_PRIVATE_KEY;

// extern const uint8_t mqtt_primary_backup_pem[] asm("_binary_mqtt_primary_backup_pem_start"); // contains both primary and backup certificates (primary first, but doesn't seem to matter)

// const uint32_t max_upload_errors = 60; // maximum upload errors before device resets
// uint32_t upload_error_count = 0;       // if the number of errors exceeds a limit, restart device

// // ------------------------------------- END MQTT

// ====================================================== STATUS

// uint8_t minute_count = 61; // Force status update on restart. Count of the minutes to know when an hour has passed (for uploading the status)
// /* on_mains_flag
//     1  = device on mains power
//     0  = device running on battery power
//     -1 = error reading the pin
// */
// int8_t on_mains_flag; // default on battery power to start??

// ------------------------------------- END STATUS

// /**
//  * Check if we are on mains or battery power
//  * Force the status update if there is a change
//  * Do this before WiFi and MQTT so the LED's don't turn on if device is on battery
//  */
// void mains_flag_evaluation(void)
// {
//     int8_t on_mains = status_onMains();
//     if (on_mains_flag != on_mains)
//     {
//         if (on_mains == 0) // Not on mains (on battery)
//         {
//             ESP_LOGW(TAG, "device on battery power");
//             on_mains_flag = 0;
//             minute_count = 61; // force the status update
//         }
//         else if (on_mains == 1)
//         {
//             ESP_LOGI(TAG, "device on mains power");
//             on_mains_flag = 1;
//             minute_count = 61; // force the status update
//         }
//         else
//         {
//             ESP_LOGE(TAG, "weird error - is the device on battery or on mains power?");
//             minute_count = 61; // force the status update
//         }
//     }
// }

// ================================================================================================= FRAM TASK



// ================================================================================================= FRAM TASK

// // // ====================================================== MQTT

// /**
//  * Check if we are on mains or battery power
//  * Force the status update if there is a change
//  * Do this before WiFi and MQTT so the LED's don't turn on if device is on battery
//  */
// void mains_flag_evaluation(void)
// {
//     int8_t on_mains = status_onMains();
//     if (on_mains_flag != on_mains)
//     {
//         if (on_mains == 0) // Not on mains (on battery)
//         {
//             ESP_LOGW(TAG, "device on battery power");
//             on_mains_flag = 0;
//             minute_count = 61; // force the status update
//         }
//         else if (on_mains == 1)
//         {
//             ESP_LOGI(TAG, "device on mains power");
//             on_mains_flag = 1;
//             minute_count = 61; // force the status update
//         }
//         else
//         {
//             ESP_LOGE(TAG, "weird error - is the device on battery or on mains power?");
//             minute_count = 61; // force the status update
//         }
//     }
// }

// /**
//  * Only call function from UPLOAD TASK
//  * Used before the restart (if restar_required_flag = true)
//  */
// void stop_wifi_and_mqtt(esp_mqtt_client_handle_t client)
// {
//     ESP_LOGW(TAG, "disconnecting mqtt");
//     esp_mqtt_client_disconnect(client);

//     ESP_LOGW(TAG, "stopping mqtt");
//     esp_mqtt_client_stop(client);

//     ESP_LOGW(TAG, "disconnecting wifi");
//     esp_wifi_disconnect();
// }

// /**
//  * Increments the upload error count (only) if telemetry publish failed
//  * Checks if the max errors has been exceeded, if it has, starts the restart process
//  */
// void increment_upload_error_count(esp_mqtt_client_handle_t client)
// {
//     upload_error_count++;
//     ESP_LOGW(TAG, "upload_error_count++");
//     if (upload_error_count > max_upload_errors)
//     {
//         ESP_LOGE(TAG, "upload_error_count > max_upload_errors, will begin the restart process");
//         stop_wifi_and_mqtt(client);
//         ESP_LOGI(TAG, "UPLOAD TASK shutdown complete, waiting for FRAM TASK");
//         restart_required_flag = true;
//         for (;;)
//         {
//         }
//     }
// }

// /**
//  * Decrements the upload_error_count if it is greater than 0
//  * Only for telemetry publish success
//  */
// void decrement_upload_error_count(void)
// {
//     if (upload_error_count > 0)
//     {
//         upload_error_count--;
//     }
// }

// // // ====================================================== MQTT

// ================================================================================================= UPLOAD TASK

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

    ESP_LOGI(TAG, "***************************************************************************************************************************");
    ESP_LOGI(TAG, "                                                FIRMWARE VERSION:  %s", CONFIG_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "***************************************************************************************************************************");

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

    // Semaphore
    status_struct_gatekeeper = xSemaphoreCreateMutex();
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
        gpio_set_level(CONFIG_WIFI_LED_PIN, 1);
    }
    gpio_set_level(CONFIG_MQTT_LED_PIN, 0);

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