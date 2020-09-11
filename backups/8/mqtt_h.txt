#ifndef _MQTT_H_
#define _MQTT_H_

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
// #include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"

#include "driver/gpio.h"

// JWT
#include "components/jwt/jwt.h"
// #include "config.h"

// Prototypes
void mqtt_init(void);
bool jwt_update_check(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);

// void get_device_id(uint8_t *device_id);
esp_err_t get_device_id(char device_id[]);

extern char device_id[20];

// Variable declerations
// const uint8_t mqtt_google_pem_start[];
const char *mqtt_google_pem_start;
const char *private_key;

#endif // _MQTT_H_