#ifndef _TASK_COMMON_H_
#define _TASK_COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"

#include "esp_event.h"
#include "freertos/queue.h" // need esp_event.h before this

// status
#include "components/status/status.h"

#include "extern_vars.h" // extern variables

extern xQueueHandle ack_queue; // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files
extern xQueueHandle upload_queue;

extern SemaphoreHandle_t status_struct_gatekeeper;

extern uint8_t minute_count;

#endif // _TASK_COMMON_H_