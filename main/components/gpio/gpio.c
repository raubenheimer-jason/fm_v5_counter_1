#include "gpio.h"

// #include <stdint.h>
// #include "driver/gpio.h"
// #include "freertos/queue.h"
// #include "esp_system.h"

// test
// #include <stdio.h>
// #include <stdint.h>
// #include <stddef.h>
// #include <string.h>
// #include "esp_system.h"
// #include "nvs_flash.h"
// #include "esp_event.h"
// #include "esp_netif.h"

// #include "esp_log.h"
// #include "mqtt_client.h"
// #include "esp_tls.h"
// #include "esp_ota_ops.h"
// test

// #include "esp_event.h" // for xSemaphoreHandle

// Static prototypes
static void IRAM_ATTR rtc_alarm_isr(void *arg);
static void IRAM_ATTR counter_isr(void *arg);
static void gpio_interrupt_init(void);
static void gpio_leds_init(void);

uint32_t count = 0;

// Semaphore for count variable
xSemaphoreHandle gatekeeper = 0;



void gpio_init(void)
{
    // initialise semaphore
    gatekeeper = xSemaphoreCreateMutex();

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_interrupt_init();
    gpio_leds_init();
}

static void IRAM_ATTR rtc_alarm_isr(void *arg)
{
    if (xSemaphoreTake(gatekeeper, 200))
    {
        // store count value in local variable
        uint32_t local_count = count;
        // local_count = count;

        // clear count
        count = 0;

        // release count
        xSemaphoreGive(gatekeeper);

        rtc_alarm_flag = true; ///////////////////////////////////////////////////////////////////////// ADD SEMAPHORE AROUND THIS GUY ?????????????

        // send count and time data to Fram Task

        // get time from esp32
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
        xSemaphoreGive(gatekeeper); // release count
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
    gpio_set_intr_type(CONFIG_COUNTER_PIN, GPIO_INTR_POSEDGE);

    // hook isr handlers for specific gpio pins
    gpio_isr_handler_add(CONFIG_RTC_ALARM_PIN, rtc_alarm_isr, (void *)CONFIG_RTC_ALARM_PIN);
    gpio_isr_handler_add(CONFIG_COUNTER_PIN, counter_isr, (void *)CONFIG_COUNTER_PIN);
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