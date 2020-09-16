#include "fram_task.h"

static const char *TAG = "FRAM_TASK";

/* on_mains_flag
    1  = device on mains power
    0  = device running on battery power
    -1 = error reading the pin
*/
int8_t on_mains_flag; // default on battery power to start??

/**
 * Check if we are on mains or battery power
 * Force the status update if there is a change
 * Do this before WiFi and MQTT so the LED's don't turn on if device is on battery
 */
void mains_flag_evaluation(void)
{
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

void Fram_Task_Code(void *pvParameters)
{
    static const char *TAG = "APP_MAIN [FRAM_TASK]";

    TickType_t fram_store_ticks = 0; // dont wait first time, there might be a backlog and the chance of a new message in the queue right on startup is unlikely

    const uint32_t short_ticks = 200;
    const uint32_t long_ticks = 10000;

    for (;;)
    {
        mains_flag_evaluation();

        uint64_t telemetry_to_store = 0;
        if (xQueueReceive(fram_store_queue, &telemetry_to_store, fram_store_ticks))
        {
            ESP_LOGD(TAG, "-------------------------- rtc alarm!! -------------------------- ");
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

            // time_t rtc_unix = rtc_get_unix();

            // time_t system_unix;
            // time(&system_unix);

            // printf("rtc_unix:     %d\n", (int)rtc_unix);
            // printf("system_unix:  %d\n", (int)system_unix);

            // if ((rtc_unix - system_unix) != 0)
            // {
            //     ESP_LOGE(TAG, "error with unix???");
            // }

            rtc_clear_alarm();
            minute_count++;
            ESP_LOGD(TAG, "minutes passed: %d", minute_count);
        }

        if (restart_required_flag == true)
        {
            ESP_LOGW(TAG, "------>> DEVICE RESTARTING IN 1 SECOND ... ");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }

        uint64_t telemetry_to_delete = 0;
        if (xQueueReceive(ack_queue, &telemetry_to_delete, 0))
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
            fram_store_ticks = short_ticks / portTICK_PERIOD_MS; // there might be a backlog so dont wait for new telemetry for long
        }
        else
        {
            fram_store_ticks = long_ticks / portTICK_PERIOD_MS; // no message backlog in FRAM, just wait for new message
        }
    }
}