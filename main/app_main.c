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

    -   Issue with RTC??    unix from rtc: 1600215542
                            unix from system time: 1600172342
                            W (25208528) NTP: RTC time needs to be updated
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
xQueueHandle upload_queue = NULL;
xQueueHandle ack_queue = NULL;

bool restart_required_flag;

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

    // printf("sizeof(time_t): %d\n", sizeof(time_t));

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

    // rtc_test();

    // WiFi
    wifi_init_sta();

    // NTP
    initialize_sntp();

    // // test RTC

    // time_t first_time = rtc_get_unix();

    // for (;;)
    // {
    //     printf("--------------------------------------------------------------------------------------------------------------\n");
    //     // print rtc time
    //     time_t rtc_unix = rtc_get_unix();

    //     time_t system_unix;
    //     time(&system_unix);

    //     printf("rtc_unix:     %d\n", (int)rtc_unix);
    //     printf("system_unix:  %d\n", (int)system_unix);

    //     if ((rtc_unix - system_unix) > 1 || (rtc_unix - system_unix) < -1)
    //     {
    //         ESP_LOGE(TAG, "error with unix???");
    //         ESP_LOGW(TAG, "waiting...");
    //         printf("first_time: %d\n", (int)first_time);
    //         for (;;)
    //         {
    //             vTaskDelay(1000 / portTICK_PERIOD_MS);
    //         }
    //     }

    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // MQTT
    mqtt_init(); // make sure NVS is initiated first (done in wifi)

    // Tasks
    start_fram_task();
    start_upload_task();
}