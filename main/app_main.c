/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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

#include "driver/gpio.h"

#include "freertos/queue.h"

static const char *TAG = "APP_MAIN";

extern const uint8_t wifi_ssid_from_file[] asm("_binary_wifi_ssid_txt_start");
extern const uint8_t wifi_password_from_file[] asm("_binary_wifi_password_txt_start");

static xQueueHandle gpio_evt_queue = NULL;

// ================================================================================================= UPLOAD TASK

void Upload_Task_Code(void *pvParameters)
{
    printf("ssid: %s\n", wifi_ssid_from_file);
    printf("pass: %s\n", wifi_password_from_file);

    printf("wifi led pin: %d\n", WIFI_LED_PIN);
    printf("mqtt led pin: %d\n", MQTT_LED_PIN);

    gpio_config_t wifi_led_config = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config_t mqtt_led_config = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&wifi_led_config);
    gpio_config(&mqtt_led_config);

    for (;;)
    {
        // Task code goes here.

        gpio_set_level(WIFI_LED_PIN, 0);
        gpio_set_level(MQTT_LED_PIN, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(WIFI_LED_PIN, 1);
        gpio_set_level(MQTT_LED_PIN, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        uint32_t io_num;
        if (xQueueReceive(gpio_evt_queue, &io_num, (100 / portTICK_PERIOD_MS)))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

// ================================================================================================= UPLOAD TASK

static void start_upload_task(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t Upload_Task = NULL;
    const uint32_t STACK_SIZE = 24000;
    const uint8_t task_priority = 10;

    // Create the task, storing the handle.  Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.  If it was just an
    // an automatic stack variable it might no longer exist, or at least have been corrupted, by the time
    // the new task attempts to access it.
    xTaskCreate(Upload_Task_Code, "Upload_Task", STACK_SIZE, &ucParameterToPass, task_priority, &Upload_Task);
    configASSERT(Upload_Task);
}

static void IRAM_ATTR counter_isr(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void counter_init(void)
{
    gpio_config_t counter_interrupt_pin_config = {
        .pin_bit_mask = GPIO_INPUT_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    gpio_config(&counter_interrupt_pin_config);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(COUNTER_PIN, counter_isr, (void *)COUNTER_PIN);
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

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    counter_init();
    start_upload_task();
}
