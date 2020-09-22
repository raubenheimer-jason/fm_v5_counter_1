// FM_V5_counter_1

// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
/*
    -   -- DONE -- Have a safety that resets the device if the number of failed mqtt messages gets too high
                   (also count down if there are successful messages, similar to winery thermometer)
    -   test restart safety ^^

    -   -- DONE -- Put primary certificate before backup in pem file (shouldn't make a difference? -- at least test)

    -   Make OTA more robust

    -   Check if sensor could see reflector or not when it gets a 0 reading

    -   -- DONE -- Publish state to GCP IOT Core?

    -   Test flash encryption etc.

    -   -- DONE -- Issue with RTC??     unix from rtc: 1600215542
                                        unix from system time: 1600172342
                                        W (25208528) NTP: RTC time needs to be updated

    -   Send config "restart" instruction??? -- more difficult than I thought because we need to make sure it doesn't constantly restart -- maybe command rather

    -   Test startup without WiFi (should get time from RTC and record counts as normal)
*/
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO

/*
    ES256

    -----BEGIN PUBLIC KEY-----
    MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIk+73DCFlHAiRJbgxGqt8loMTMD8
    woXrlldwibH6K5gRdqn5CpdKahEtcHqgi7z7pr7ioaq4a714bzKNeT3Yug==
    -----END PUBLIC KEY-----

    DEVICE_PRIVATE_KEY
    ab:05:1e:33:36:d9:b0:1e:b2:00:1a:b2:da:1c:21:84:bf:ee:46:5e:3a:7d:3f:11:1f:73:a6:4b:bd:d7:5d:f6
*/

#include "main.h"

// #include "esp_heap_trace.h"

// #define NUM_RECORDS 100
// static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

// /* For printing uint64_t */
// #define __STDC_FORMAT_MACROS
// #include <inttypes.h>
// uint64_t i;
// printf("\t\tt.addr: %" PRIu64 "\n", t.addr);

static const uint32_t minimum_battery_voltage_allowed = 3100; // mV - if the battery voltage is below this threshold, there will be a delay before the device starts up. This is to prevent the constant restart on brownout if the batteries run flat.

static const char *TAG = "APP_MAIN";

xSemaphoreHandle status_struct_gatekeeper = NULL;

xQueueHandle fram_store_queue = NULL;
xQueueHandle upload_queue = NULL;
xQueueHandle ack_queue = NULL;

static void start_upload_task(void)
{
    TaskHandle_t Upload_Task = NULL;
    const uint32_t STACK_SIZE = 32000;
    const uint8_t task_priority = 10;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate(Upload_Task_Code, "Upload_Task", STACK_SIZE, NULL, task_priority, &Upload_Task);
    configASSERT(Upload_Task);
}

static void start_fram_task()
{
    TaskHandle_t Fram_Task = NULL;
    const uint32_t STACK_SIZE = 8000;
    const uint8_t task_priority = 9;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate(Fram_Task_Code, "Fram_Task", STACK_SIZE, NULL, task_priority, &Fram_Task);
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
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_log_level_set("APP_MAIN", ESP_LOG_VERBOSE);

    ESP_LOGI(TAG, "***************************************************************************************************************************");
    ESP_LOGI(TAG, "                                                FIRMWARE VERSION:  %s", CONFIG_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "***************************************************************************************************************************");

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));

    // FRAM
    fram_spi_init();

    // create queues
    fram_store_queue = xQueueCreate(120, sizeof(uint64_t));
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

    /*  If device is on battery and the voltage is below a threshold, delay before attempting startup
        to help prevent continuous restarts after brownout when battery is flat
    */
    if (status_onMains() != 1 && get_battery_voltage() < minimum_battery_voltage_allowed)
    {
        ESP_LOGW(TAG, "status_onMains() != 1 && get_battery_voltage() < minimum_battery_voltage_allowed, delaying before attempted startup");
        ESP_LOGW(TAG, "status_onMains() = %d, get_battery_voltage() = %d", status_onMains(), get_battery_voltage());
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    if (need_to_restart == true)
    {
        ESP_LOGE(TAG, "restarting in 10s");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    on_mains_flag = -1; // force the evaluation
    mains_flag_evaluation();
    if (on_mains_flag == 0)
    {
        ESP_LOGW(TAG, "battery voltage = %d", get_battery_voltage());
    }

    // Initial LED states
    if (on_mains_flag == 1) // only turn the LED on if device is on mains power
    {
        gpio_set_level(CONFIG_WIFI_LED_PIN, 1);
    }
    gpio_set_level(CONFIG_MQTT_LED_PIN, 0);

    // Time
    time_init();

    // Tasks
    start_fram_task();
    start_upload_task();
}