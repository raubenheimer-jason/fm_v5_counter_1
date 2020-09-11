#include "time_init.h"

static const char *TAG = "TIME_INIT";

void time_init(void)
{
    rtc_begin(CONFIG_I2C_SCL_PIN, CONFIG_I2C_SDA_PIN);

    /*
        If RTC time is valid (OSF == 0), update system time from RTC
            RTC time will be updated as soon as NTP callback is triggered (this is where the OSF is cleared in RTC)
            If NTP callback isn't triggered often enough, RTC must update system time if system time is wrong
    */

    uint32_t rtc_unix = rtc_get_unix();

    if (rtc_unix > CONFIG_LAST_KNOWN_UNIX) // OSF == 0 and time is probably valid
    {
        // Set system time from rtc
        struct timeval tv;
        tv.tv_sec = rtc_unix;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    rtc_config_alarm();
    rtc_clear_alarm();

    ESP_LOGI(TAG, "time initialisation done");
}