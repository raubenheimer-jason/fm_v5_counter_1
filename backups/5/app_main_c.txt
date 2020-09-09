/* FM_V5_counter_1
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"

#include "config.h"
#include "main.h"

#include "driver/gpio.h"

#include "freertos/queue.h"

#include "components/rtc/rtc.h"
#include "components/fram/fram.h"

#include <time.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
// uint64_t i;
// printf("\t\tt.addr: %" PRIu64 "\n", t.addr);

static const char *TAG = "APP_MAIN";

extern const uint8_t wifi_ssid_from_file[] asm("_binary_wifi_ssid_txt_start");
extern const uint8_t wifi_password_from_file[] asm("_binary_wifi_password_txt_start");

// Semaphore for count variable
xSemaphoreHandle gatekeeper = 0;

static xQueueHandle fram_store_queue = NULL;
static xQueueHandle upload_queue = NULL;
static xQueueHandle ack_queue = NULL;

bool rtc_alarm_flag = false;

// ================================================================================================= FRAM TASK

void Fram_Task_Code(void *pvParameters)
{
    // uint64_t previously_sent_telemetry = 1;
    for (;;)
    {
        if (rtc_alarm_flag == true)
        {
            ESP_LOGI(TAG, "-------------------------- rtc alarm!! -------------------------- ");
            rtc_alarm_flag = false;
            rtc_clear_alarm();
            // uint32_t unix_now = rtc_get_unix();
            // printf("unix now: %d\n", unix_now);
            // printf("count: %d\n", count);
            // count = 0;
        }

        uint64_t telemetry_to_store = 0;
        if (xQueueReceive(fram_store_queue, &telemetry_to_store, 0))
        {
            ESP_LOGI(TAG, "received telemetry_to_store");

            // uint32_t telemetry_unix = telemetry_to_store >> 32;
            // uint32_t telemetry_count = (uint32_t)telemetry_to_store;

            // printf("telemetry_unix:  %d\n", telemetry_unix);
            // printf("telemetry_count: %d\n", telemetry_count);

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
            if (telemetry_to_delete > last_known_unix)
            {
                // uint32_t telemetry_unix = telemetry_to_store >> 32;
                // uint32_t telemetry_count = (uint32_t)telemetry_to_store;

                // printf("telemetry_unix:  %d\n", telemetry_unix);
                // printf("telemetry_count: %d\n", telemetry_count);

                uint32_t unix = telemetry_to_delete >> 32;
                uint32_t count = (uint32_t)telemetry_to_delete;

                // Store telemetry in FRAM
                if (delete_last_read_telemetry(telemetry_to_delete) == true)
                // {
                //     ESP_LOGE(TAG, "error deleting telemetry (unix: %d, count: %d)", unix, count);
                // }
                // else
                {
                    ESP_LOGI(TAG, "telemetry successfully deleted from FRAM");
                }
            }
        }

        uint64_t telemetry_to_upload = read_telemetry();

        if (telemetry_to_upload > 0) // && telemetry_to_upload != previously_sent_telemetry)
        {
            ESP_LOGI(TAG, "send telemetry from Fram_Task to Upload_Task");

            uint32_t unix_time = telemetry_to_upload >> 32;

            if (unix_time > last_known_unix)
            {
                if (xQueueSend(upload_queue, &telemetry_to_upload, 0))
                {
                    ESP_LOGI(TAG, "telemetry sent to Upload_Task");
                    // previously_sent_telemetry = telemetry_to_upload;
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
        else
        {
            ESP_LOGW(TAG, "telemetry_to_upload == 0");
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        // rtc_print_status_register();

        // vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}

// ================================================================================================= FRAM TASK

// ================================================================================================= UPLOAD TASK

void Upload_Task_Code(void *pvParameters)
{
    printf("ssid: %s\n", wifi_ssid_from_file);
    printf("pass: %s\n", wifi_password_from_file);

    // printf("wifi led pin: %d\n", WIFI_LED_PIN);
    // printf("mqtt led pin: %d\n", MQTT_LED_PIN);

    // gpio_config_t wifi_led_config = {
    //     .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = 0,
    //     .pull_down_en = 0,
    //     .intr_type = GPIO_INTR_DISABLE,
    // };

    // gpio_config_t mqtt_led_config = {
    //     .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = 0,
    //     .pull_down_en = 0,
    //     .intr_type = GPIO_INTR_DISABLE,
    // };

    // gpio_config(&wifi_led_config);
    // gpio_config(&mqtt_led_config);

    bool need_to_upload_flag = false;
    uint64_t previously_uploaded_telemetry = 0;

    for (;;)
    {
        gpio_set_level(WIFI_LED_PIN, 0);
        gpio_set_level(MQTT_LED_PIN, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(WIFI_LED_PIN, 1);
        gpio_set_level(MQTT_LED_PIN, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        uint64_t telemetry_to_upload = 0;

        if (need_to_upload_flag == false && xQueueReceive(upload_queue, &telemetry_to_upload, 0))
        {
            if (telemetry_to_upload == previously_uploaded_telemetry)
            {
                uint32_t unix_time = telemetry_to_upload >> 32;
                uint32_t count = (uint32_t)telemetry_to_upload;

                // uint64_t i;
                // printf("\t\tt.addr: %" PRIu64 "\n", t.addr);
                ESP_LOGW(TAG, "telemetry_to_upload == previously_uploaded_telemetry (unix: %d, count: %d)", unix_time, count);
                if (xQueueSend(ack_queue, &telemetry_to_upload, portMAX_DELAY)) // SHOULD THIS BE MAX DELAY??????????????????
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
            // String telemetry = "{\"unix_timestamp\":" + String(unix_time) + ",\"value\":" + String(count) + "}";

            // {"t":1596255206,"v":4294967295} --> 31 characters
            char telemetry_buf[40];
            snprintf(telemetry_buf, 40, "{\"t\":%d,\"v\":%d}", (int)unix_time, (int)count);

            printf("%s\n", telemetry_buf);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            // String telemetry = "{\"t\":" + String(unix_time) + ",\"v\":" + String(count) + "}";
            // std::string telemetry = "{\"t\":" + String(unix_time) + ",\"v\":" + String(count) + "}";

            // keep these split up (try prevent weird error where success was true but wifi had just turned off so the message wasnt sent)
            // bool upload_success{false};
            // // upload_success = publishTelemetry(telemetry);
            // upload_success = publishTelemetry(telemetry.c_str());

            // uint32_t upload_res{publishTelemetry(telemetry.c_str())};
            // uint32_t upload_res{publishTelemetry(telemetry_buf)};

            bool upload_res = true;

            if (upload_res == true)
            {
                // status.insert_upload_time(upload_res);
                // success_count++;
                // digitalWrite(aux_led_pin, HIGH);
                // mqtt_last_connect_time = millis();

                // // Serial.print(" |  [UPLOAD] SUCCESS! --> " + telemetry);
                // ESP_LOGD(TAG, "UPLOAD SUCCESS! --> %s", telemetry_buf);
                // // upload_decrement_sending_failed_count(mqtt_sending_failed_count);
                // upload_decrement_sending_failed_count(http_sending_failed_count);
                previously_uploaded_telemetry = telemetry_to_upload;
                need_to_upload_flag = false;

                // if sending to GCP was successful
                //? what happens if sending to GCP fails? / now we have removed the value from the queue...
                //? it's still in the variable though so just make sure to upload it... (maybe have upload_success_flag ?)
                //? if the power is lost after the telemetry is received from the queue, that's fine, the ack wouldnt have been sent to FRAM if the data hasn't been uploaded to GCP
                //?   --> next time the device turns on, FRAM will send the same message back to UPLOAD because it didnt delete it because it didnt receive the ack
                // Send ack to FRAM (the unix that was uploaded to GCP)
                // if (!xQueueSend(ackQueue, &unix_time, 1000 / portTICK_PERIOD_MS)) //? dont think we need to wait here, if the queue is full there is a biggere problem...
                if (!xQueueSend(ack_queue, &telemetry_to_upload, 1000 / portTICK_PERIOD_MS)) //? dont think we need to wait here, if the queue is full there is a biggere problem...
                {
                    // Serial.print("[UPLOAD] adding to ackQueue failed !!!");
                    ESP_LOGE(TAG, "adding to ackQueue failed !!! "); //-- %d", telemetry_to_upload);
                }
            }
            else
            {
                // fail_count++;
                // TODO try connect to mqtt here if it fails..... probably lost the connection now we're stuck in the loop
                // Serial.print("  |  [UPLOAD] SENDING FAILED !!!");
                ESP_LOGE(TAG, "UPLOAD SENDING FAILED !!!"); // -- %d      %.0f/%.0f (%.2f %)  |  fh: %d  min fh: %d", receive_telemetry, success_count, (success_count + fail_count), ((success_count / (fail_count + success_count)) * 100), xPortGetFreeHeapSize(), esp_get_minimum_free_heap_size());
                // digitalWrite(aux_led_pin, LOW);
                // upload_increment_sending_failed_count(mqtt_sending_failed_count);
                // upload_increment_sending_failed_count(http_sending_failed_count);
                // status.increment_telemetry_publish_fail_count();
                // delay(10000);
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

static void IRAM_ATTR rtc_alarm_isr(void *arg)
{
    if (xSemaphoreTake(gatekeeper, 200))
    {
        uint32_t local_count;

        // store count value in local variable
        local_count = count;

        // clear count
        count = 0;

        // release count
        xSemaphoreGive(gatekeeper);

        rtc_alarm_flag = true;

        // send count and time data to Fram Task

        // get time from esp32
        // uint32_t unix_now = 123456789;
        time_t unix_now;
        time(&unix_now);

        // store time and count in uint64_t
        uint64_t telemetry = (uint64_t)unix_now << 32 | local_count;

        // send telemetry to fram_queue
        xQueueSendFromISR(fram_store_queue, &telemetry, NULL);
    }
}

static void IRAM_ATTR counter_isr(void *arg)
{
    if (xSemaphoreTake(gatekeeper, 200))
    {
        count++;

        // release count
        xSemaphoreGive(gatekeeper);
    }
}

/**
 * Initialise the gpio interrupts
 * (RTC alarm & counter)
 */
static void gpio_interrupt_init(void)
{
    gpio_config_t gpio_interrupt_pin_config = {
        .pin_bit_mask = GPIO_INPUT_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    gpio_config(&gpio_interrupt_pin_config);

    // Change the interrupt type for the counter pin
    gpio_set_intr_type(COUNTER_PIN, GPIO_INTR_POSEDGE);

    // hook isr handlers for specific gpio pins
    gpio_isr_handler_add(RTC_ALARM_PIN, rtc_alarm_isr, (void *)RTC_ALARM_PIN);
    gpio_isr_handler_add(COUNTER_PIN, counter_isr, (void *)COUNTER_PIN);
}

/**
 * Initialise the LED gpios
 * (red and blue LED)
 */
static void gpio_leds_init(void)
{
    gpio_config_t led_gpios_config = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_gpios_config);
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

    // fram_spi_init(SPI_MOSI_PIN, SPI_MISO_PIN, SPI_CLK_PIN, SPI_CS_PIN, FRAM_HOST);
    fram_spi_init();

    // printf("** FRAM stuff done **\n");

    // initialise semaphore
    gatekeeper = xSemaphoreCreateMutex();

    // create queues
    fram_store_queue = xQueueCreate(10, sizeof(uint64_t));
    upload_queue = xQueueCreate(1, sizeof(uint64_t));
    ack_queue = xQueueCreate(1, sizeof(uint64_t));

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_interrupt_init();
    gpio_leds_init();

    start_fram_task();

    // for (;;)
    // {
    //     vTaskDelay(100);
    // }

    start_upload_task();

    rtc_begin(I2C_SCL_PIN, I2C_SDA_PIN);

    uint32_t unix_to_set = 1599553793;
    rtc_set_date_time(&unix_to_set);
    rtc_config_alarm();
    rtc_clear_alarm();
}

// FRAM ======================================================================================= FRAM
/*
// test();

// fram_reset();

// uint32_t max = 5;
uint32_t start = 0;
// uint32_t max = 15999; // max number of messages
uint32_t max = 16000;
// uint32_t max = 1500;

for (uint32_t i = start; i < max; i++)
{
    uint32_t unix = i;
    uint32_t count = i;

    uint64_t write_tel = (uint64_t)unix << 32 | count;

    // write_telemetry((uint64_t)0);
    write_telemetry(write_tel);

    // printf("delay (i = %d)  \ttel = %d  \t", i, (uint32_t)tel);
    // display_top_bottom();

    // if (i % 1000 == 0 && i > 0)
    // {
    //     vTaskDelay(10);
    // }

    if ((i % 1000 == 0 && i > 0) || i == (max - 1))
    {
        vTaskDelay(2);
        printf("delay (i = %d)\tunix = %d  count = %d  \t", i, unix, count);
        display_top_bottom();
    }

    uint64_t read_tel = read_telemetry();

    // printf("delay (i = %d)  \ttel = %d\n", i, (uint32_t)tel);

    delete_last_read_telemetry(read_tel);

    // if (i % 1000 == 0 && i > 0)
    // {
    //     vTaskDelay(10);
    // }

    if ((i % 1000 == 0 && i > 0) || i == (max - 1))
    {

        uint32_t unix = read_tel >> 32;
        uint32_t count = (uint32_t)read_tel;
        vTaskDelay(2);
        printf("\t\t\t\t\t\t\t\t\t\tread: \tunix = %d  count = %d  \t", unix, count);
        display_top_bottom();
    }
}
printf("\n");

check_state();

// printf("telemetry written\n\n");

// for (uint32_t i = start; i < max; i++)
// {
//     uint64_t tel = read_telemetry();

//     // printf("delay (i = %d)  \ttel = %d\n", i, (uint32_t)tel);

//     delete_last_read_telemetry(tel);

//     // if (i % 1000 == 0 && i > 0)
//     // {
//     //     vTaskDelay(10);
//     // }

//     if ((i % 1000 == 0 && i > 0) || i == (max - 1))
//     {

//         uint32_t unix = tel >> 32;
//         uint32_t count = (uint32_t)tel;
//         vTaskDelay(2);
//         printf("delay (i = %d)\tunix = %d  count = %d  \t", i, unix, count);
//         display_top_bottom();
//     }
// }
// printf("\n");

// check_state();
// printf("telemetry read\n");

for (;;)
{
    vTaskDelay(1);
}
*/
// FRAM ======================================================================================= FRAM
