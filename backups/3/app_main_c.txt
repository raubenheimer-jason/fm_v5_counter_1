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

static const char *TAG = "APP_MAIN";

extern const uint8_t wifi_ssid_from_file[] asm("_binary_wifi_ssid_txt_start");
extern const uint8_t wifi_password_from_file[] asm("_binary_wifi_password_txt_start");

// Semaphore for count variable
xSemaphoreHandle gatekeeper = 0;

static xQueueHandle fram_store_queue = NULL;
// static xQueueHandle upload_queue = NULL;
// static xQueueHandle ack_queue = NULL;

bool rtc_alarm_flag = false;

// ================================================================================================= FRAM TASK

void Fram_Task_Code(void *pvParameters)
{
    for (;;)
    {
        if (rtc_alarm_flag == true)
        {
            printf("--------------------- rtc alarm!! --------------------- \n");
            rtc_alarm_flag = false;
            rtc_clear_alarm();
            // uint32_t unix_now = rtc_get_unix();
            // printf("unix now: %d\n", unix_now);
            // printf("count: %d\n", count);
            // count = 0;
        }

        uint64_t telemetry_to_store;
        if (xQueueReceive(fram_store_queue, &telemetry_to_store, (100 / portTICK_PERIOD_MS)))
        {
            printf("received telemetry_to_store\n");

            uint32_t telemetry_unix = telemetry_to_store >> 32;
            uint32_t telemetry_count = (uint32_t)telemetry_to_store;

            printf("telemetry_unix:  %d\n", telemetry_unix);
            printf("telemetry_count: %d\n", telemetry_count);
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);

        // rtc_print_status_register();

        // vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}

// ================================================================================================= FRAM TASK

// ================================================================================================= UPLOAD TASK

// void Upload_Task_Code(void *pvParameters)
// {
//     printf("ssid: %s\n", wifi_ssid_from_file);
//     printf("pass: %s\n", wifi_password_from_file);

//     printf("wifi led pin: %d\n", WIFI_LED_PIN);
//     printf("mqtt led pin: %d\n", MQTT_LED_PIN);

//     gpio_config_t wifi_led_config = {
//         .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
//         .mode = GPIO_MODE_OUTPUT,
//         .pull_up_en = 0,
//         .pull_down_en = 0,
//         .intr_type = GPIO_INTR_DISABLE,
//     };

//     gpio_config_t mqtt_led_config = {
//         .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
//         .mode = GPIO_MODE_OUTPUT,
//         .pull_up_en = 0,
//         .pull_down_en = 0,
//         .intr_type = GPIO_INTR_DISABLE,
//     };

//     gpio_config(&wifi_led_config);
//     gpio_config(&mqtt_led_config);

//     for (;;)
//     {
//         gpio_set_level(WIFI_LED_PIN, 0);
//         gpio_set_level(MQTT_LED_PIN, 1);
//         vTaskDelay(500 / portTICK_PERIOD_MS);

//         gpio_set_level(WIFI_LED_PIN, 1);
//         gpio_set_level(MQTT_LED_PIN, 0);
//         vTaskDelay(500 / portTICK_PERIOD_MS);

//         uint64_t telemetry_to_upload;
//         if (xQueueReceive(upload_queue, &telemetry_to_upload, (100 / portTICK_PERIOD_MS)))
//         {
//             printf("received telemetry to upload");
//         }
//     }
// }

// ================================================================================================= UPLOAD TASK

// static void start_upload_task(void)
// {
//     static uint8_t ucParameterToPass;
//     TaskHandle_t Upload_Task = NULL;
//     const uint32_t STACK_SIZE = 24000;
//     // const uint8_t task_priority = tskIDLE_PRIORITY;
//     const uint8_t task_priority = 10;

//     // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
//     // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
//     // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
//     // the new task attempts to access it.
//     xTaskCreate(Upload_Task_Code, "Upload_Task", STACK_SIZE, &ucParameterToPass, task_priority, &Upload_Task);
//     configASSERT(Upload_Task);
// }

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

static void fram_spi_init()
{
    spi_device_handle_t spi_device;

    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing bus SPI...");
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_CLK_PIN,
        // .quadwp_io_num = -1,
        // .quadhd_io_num = -1,
        // .max_transfer_sz = 128,
    };
    //Initialize the SPI bus
    ret = spi_bus_initialize(FRAM_HOST, &buscfg, 0);
    ESP_ERROR_CHECK(ret);
    printf("spi_bus_initialize ret: %d\n", ret);

    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 24,
        // .dummy_bits = 8,
        .mode = 0, //SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
        .clock_speed_hz = 1 * 1000 * 1000, //Clock out at 10 MHz
        .input_delay_ns = 0,
        .spics_io_num = SPI_CS_PIN, //CS pin
        // .spics_io_num = -1,
        .queue_size = 2, //We want to be able to queue 7 transactions at a time
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    //Attach the FRAM to the SPI bus
    ret = spi_bus_add_device(FRAM_HOST, &devcfg, &spi_device);
    ESP_ERROR_CHECK(ret);
    printf("spi_bus_add_device ret: %d\n", ret);

    // cs_init();

    // vTaskDelay(100 / portTICK_PERIOD_MS);

    uint8_t status_reg = read_status_register(spi_device);
    printf("\n---------------------\n");
    // printf("status reg raw:\t%d\n", status_reg);
    printf("status reg:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_reg));

    uint8_t byte_to_write = 0b00110011;
    uint32_t address_to_write = 100;

    // fram_write_byte(spi_device, address_to_write, byte_to_write);

    uint8_t test_byte = fram_read_byte(spi_device, address_to_write);
    printf("\n---------------------\n");
    // printf("write byte 0:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(byte_to_write));
    printf("read byte 0:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test_byte));

    while (1)
    {
        // Add your main loop handling code here.
        vTaskDelay(1);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    fram_spi_init();

    printf("SHOULDNT GET HERE!!!\n");

    // initialise semaphore
    gatekeeper = xSemaphoreCreateMutex();

    // create queues
    fram_store_queue = xQueueCreate(10, sizeof(uint64_t));
    // upload_queue = xQueueCreate(1, sizeof(uint64_t));
    // ack_queue = xQueueCreate(1, sizeof(uint64_t));

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_interrupt_init();

    // start_upload_task();
    start_fram_task();

    rtc_begin(I2C_SCL_PIN, I2C_SDA_PIN);

    uint32_t unix_to_set = 1599553793;
    rtc_set_date_time(&unix_to_set);
    rtc_config_alarm();
    rtc_clear_alarm();
}
