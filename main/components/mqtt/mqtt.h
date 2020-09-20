#ifndef _MQTT_H_
#define _MQTT_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"

// OTA update -- >https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "driver/gpio.h"

// JWT
#include "components/jwt/jwt.h"

// GPIO
#include "components/gpio/gpio.h" // for on_mains_flag

#include "extern_vars.h" // extern variables

// Prototypes
void mqtt_init(void);
bool jwt_update_check(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);

esp_err_t get_device_id(char device_id[]);

extern char device_id[20];

extern int8_t mqtt_connected_flag; // 1 = connected, 0 = not connected // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files

#endif // _MQTT_H_