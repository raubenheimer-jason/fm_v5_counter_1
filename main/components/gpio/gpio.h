#ifndef _GPIO_H_
#define _GPIO_H_

// #include "esp_system.h"
#include "esp_event.h" // for xSemaphoreHandle (this also includes "freertos/FreeRTOS.h" which is needed for "freertos/queue.h")
#include <stdint.h>
#include "driver/gpio.h"
// #include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "../../main.h" // for the extern variables

// GPIO
#define GPIO_OUTPUT_PIN_BITMASK ((1ULL << CONFIG_WIFI_LED_PIN) | (1ULL << CONFIG_MQTT_LED_PIN))
#define GPIO_INPUT_PIN_BITMASK ((1ULL << CONFIG_COUNTER_PIN) | (1ULL << CONFIG_RTC_ALARM_PIN))
#define ESP_INTR_FLAG_DEFAULT 0

// #include "esp_netif.h"

// #include "esp_log.h"
// #include "mqtt_client.h"
// #include "esp_tls.h"
// #include "esp_ota_ops.h"

// // #include "config.h"
// #include "main.h"

// #include "freertos/queue.h"

// Prototypes
void gpio_init(void);

#endif // _GPIO_H_