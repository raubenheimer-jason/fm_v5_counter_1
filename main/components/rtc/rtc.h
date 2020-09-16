#ifndef _RTC_H_
#define _RTC_H_

// DS3231

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "time.h"

#define INCLUDE_RTC_TEST 0     // dont include the rtc test function unless we need to use it (just dont want to delete it)
#define INCLUDE_UINT8_TO_BCD 0 // dont include unless we need to use it (just dont want to delete it)  (uint8_to_bcd)

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
esp_err_t rtc_set_date_time(const time_t *unix);
uint32_t rtc_get_unix();
esp_err_t rtc_begin(uint8_t scl_pin, uint8_t sda_pin);

#if INCLUDE_RTC_TEST
void rtc_test(void);
#endif // INCLUDE_RTC_TEST

void rtc_print_status_register(void);
void rtc_print_control_register(void);

void rtc_config_alarm();
void rtc_clear_alarm();

#endif // _RTC_H_