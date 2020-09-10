#ifndef _NTP_H_
#define _NTP_H_

#include <time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "components/rtc/rtc.h"

// Prototypes
void time_sync_notification_cb(struct timeval *tv);
void initialize_sntp(void);

#endif // _NTP_H