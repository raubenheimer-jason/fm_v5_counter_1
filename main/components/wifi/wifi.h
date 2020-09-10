#ifndef _WIFI_H_
#define _WIFI_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// #include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Prototypes
void wifi_init_sta(void);

#endif // _WIFI_H_