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
// power
void status_evaluatePower(void);
// wifi
void status_setRssi(int8_t rssi);
void status_incrementWifiDisconnections(void);
void status_clearWifiDisconnections(void);
// mqtt
void status_incrementMqttUploadErrors(void);
void status_clearMqttUploadErrors(void);
void status_incrementMqttConfigReadErrors(void);
void status_clearMqttConfigReadErrors(void);
// time
void status_incrementNtpErrors(void);
void status_clearNtpErrors(void);
void status_incrementSysTimeUpdateErrors(void);
void status_clearSysTimeUpdateErrors(void);
void status_incrementRtcSetErrors(void);
void status_clearRtcSetErrors(void);
void status_incrementRtcReadErrors(void);
void status_clearRtcReadErrors(void);
// fram
void status_incrementFramReadErrors(void);
void status_clearFramReadErrors(void);
void status_incrementFramWriteErrors(void);
void status_clearFramWriteErrors(void);
void status_incrementFramAlignmentErrors(void);
void status_clearFramAlignmentErrors(void);
void status_framHighWaterMark(uint32_t num_messages);
void status_clearframHighWaterMark(void);

// char *get_status_message(void);
void status_printStatusMessage(void);

#endif // _STATUS_H_