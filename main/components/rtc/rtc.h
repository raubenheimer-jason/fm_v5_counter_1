#ifndef _RTC_H_
#define _RTC_H_

// DS3231

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "time.h"

// #include "../../config.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

// Non-static prototypes
esp_err_t rtc_set_date_time(uint32_t *unix);
uint32_t rtc_get_unix(); //, uint32_t *unix)
void rtc_test(void);
esp_err_t rtc_begin(uint8_t scl_pin, uint8_t sda_pin);

void rtc_print_status_register(void);
void rtc_print_control_register(void);

#endif // _RTC_H_