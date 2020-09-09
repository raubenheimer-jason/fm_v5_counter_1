#ifndef _CONFIG_H_
#define _CONFIG_H_

// File contains all the config data (such as GPIO pin numbers)

#include <stdint.h>

// WIFI
#define WIFI_SSID "VodafoneMobileWiFi-9ADC"
#define WIFI_PASSWORD "44SChscH"

// const uint8_t *wifi_ssid = "VodafoneMobileWiFi-9ADC";
// const uint8_t *wifi_password = "44SChscH";

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
// const uint32_t last_known_unix = 1598008484;
const uint32_t last_known_unix = 2;

// // FRAM SPI
// // pins
// const uint8_t SPI_MOSI_PIN = 36;
// const uint8_t SPI_MISO_PIN = 34;
// const uint8_t SPI_CLK_PIN = 35;
// const uint8_t SPI_CS_PIN = 33;
// // host
// const uint8_t FRAM_HOST = SPI2_HOST;
// // data sizes etc.
// const uint8_t telemetry_bytes = 8;
// // ((8000-8)/ 8) == 999 --> so memory address limit top = 8000-8 = 7992
// // ((128,000-8)/ 8) == 15,999 -- > so memory address limit top = 128,000-8 = 127,992
// const uint32_t fram_limit_top = 127992;
// const uint8_t fram_limit_bottom = 4;

#endif // _CONFIG_H_