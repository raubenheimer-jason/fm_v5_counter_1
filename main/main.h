#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"

#include "main.h"

#include "freertos/queue.h"

// Time
#include "components/time_init/time_init.h"

// FRAM
#include "components/fram/fram.h"

// wifi
#include "components/wifi/wifi.h"

// NTP
#include "components/ntp/ntp.h"

// GPIO
#include "components/gpio/gpio.h"

// MQTT
#include "components/mqtt/mqtt.h"

// status
#include "components/status/status.h"

extern xQueueHandle fram_store_queue; // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files
extern SemaphoreHandle_t rtc_alarm_flag_gatekeeper;

#endif // _MAIN_H_