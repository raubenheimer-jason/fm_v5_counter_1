#ifndef _GPIO_H_
#define _GPIO_H_

#include "esp_event.h" // for xSemaphoreHandle (this also includes "freertos/FreeRTOS.h" which is needed for "freertos/queue.h")
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "../../main.h" // for the extern variables

// GPIO
#define GPIO_OUTPUT_PIN_BITMASK ((1ULL << CONFIG_WIFI_LED_PIN) | (1ULL << CONFIG_MQTT_LED_PIN))
#define GPIO_INPUT_PIN_BITMASK ((1ULL << CONFIG_COUNTER_PIN) | (1ULL << CONFIG_RTC_ALARM_PIN) | (1ULL << CONFIG_BAT_STATUS_PIN) | (1ULL << CONFIG_MAINS_SENSE_PIN))
#define ESP_INTR_FLAG_DEFAULT 0

// Prototypes
esp_err_t gpio_initial_setup(void);

// Declerations
extern int8_t on_mains_flag; // Don't have any LED's on when on battery power

extern xQueueHandle fram_store_queue; // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files

#endif // _GPIO_H_