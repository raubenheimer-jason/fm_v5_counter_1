#ifndef _CONFIG_H_
#define _CONFIG_H_

// File contains all the config data (such as GPIO pin numbers)

#include <stdint.h>

#define GPIO_OUTPUT_PIN_BITMASK ((1ULL << WIFI_LED_PIN) | (1ULL << MQTT_LED_PIN))
#define GPIO_INPUT_PIN_BITMASK ((1ULL << COUNTER_PIN) | (1ULL << RTC_ALARM_PIN))

#define ESP_INTR_FLAG_DEFAULT 0

// LED pins
const uint8_t WIFI_LED_PIN = 1;
const uint8_t MQTT_LED_PIN = 2;

// Counter interrupt pin
const uint8_t COUNTER_PIN = 19;

// RTC alarm interrupt pin
const uint8_t RTC_ALARM_PIN = 20;

// RTC I2C pins
const uint8_t I2C_SCL_PIN = 40;
const uint8_t I2C_SDA_PIN = 39;

// Time
const uint32_t last_known_unix = 1598008484;

#endif // _CONFIG_H_