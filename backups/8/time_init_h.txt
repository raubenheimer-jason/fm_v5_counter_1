#ifndef _TIME_INIT_H_
#define _TIME_INIT_H_

#include <stdint.h>
#include <sys/time.h>
#include "esp_log.h"

// RTC
#include "components/rtc/rtc.h"

// Prototypes
void time_init(void);

#endif // _TIME_INIT_H_