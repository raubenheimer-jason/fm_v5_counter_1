#include "ntp.h"

static const char *TAG = "NTP";

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "************* Notification of a time synchronization event *************");

    // Need to check if RTC needs updating
    uint32_t rtc_unix = rtc_get_unix();

    ESP_LOGD(TAG, "unix from rtc: %d", rtc_unix);

    time_t unix_now;
    time(&unix_now);

    ESP_LOGD(TAG, "unix from system time: %d", (uint32_t)unix_now);

    const uint8_t time_tolerance = 1; // seconds (actually 2 because it isn't = to) ??

    if (rtc_unix < ((uint32_t)unix_now - time_tolerance) || rtc_unix > ((uint32_t)unix_now + time_tolerance))
    {
        ESP_LOGE(TAG, "-------------------------------------------- RTC time needs to be updated --------------------------------------------");
        ESP_LOGE(TAG, "unix from system time: %d, unix from rtc: %d", (uint32_t)unix_now, rtc_unix);

        time(&unix_now); // make sure unix_now is up to date
        esp_err_t ret = rtc_set_date_time(&unix_now);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "error updating RTC time");
        }
        else
        {
            ESP_LOGI(TAG, "RTC updated from system time (NTP)");
        }

        rtc_unix = rtc_get_unix();

        ESP_LOGD(TAG, "unix from rtc after update: %d", rtc_unix);
    }
}