#ifndef _STATUS_H_
#define _STATUS_H_

#include <stdint.h>
// #include <stdio.h>
// #include <stddef.h>
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/adc.h"
// #include "esp_event.h" // for vTaskDelay()


// Prototypes

void status_evaluatePower(void);

// char *get_status_message(void);
void status_printStatusMessage(void);

#endif // _STATUS_H_