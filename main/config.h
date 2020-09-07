#ifndef _CONFIG_H_
#define _CONFIG_H_

// File contains all the config data (such as GPIO pin numbers)

#include <stdint.h>

#define GPIO_OUTPUT_PIN_BITMASK ((1ULL << WIFI_LED_PIN) | (1ULL << MQTT_LED_PIN))
#define GPIO_INPUT_PIN_BITMASK ((1ULL << COUNTER_PIN))

#define ESP_INTR_FLAG_DEFAULT 0

// LEDs
const uint8_t WIFI_LED_PIN = 1;
const uint8_t MQTT_LED_PIN = 2;

// Counter interrupt
const uint8_t COUNTER_PIN = 19;

#endif // _CONFIG_H_