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
void status_resetStruct(void);
// char *get_status_message(void);
void status_printStatusStruct(void);

// power
int8_t status_onMains(void);
void status_evaluatePower(void);
// wifi
void status_setRssiLowWaterMark(int8_t rssi);  // added
void status_incrementWifiDisconnections(void); // added
// void status_clearWifiDisconnections(void);
// mqtt
void status_incrementMqttUploadErrors(void); // added
// void status_clearMqttUploadErrors(void);
void status_incrementMqttConfigReadErrors(void);
// void status_clearMqttConfigReadErrors(void);
// time
void status_incrementNtpErrors(void);
// void status_clearNtpErrors(void);
void status_incrementSysTimeUpdateErrors(void);
// void status_clearSysTimeUpdateErrors(void);
void status_incrementRtcSetErrors(void);
// void status_clearRtcSetErrors(void);
void status_incrementRtcReadErrors(void);
// void status_clearRtcReadErrors(void);
// fram
void status_incrementFramReadErrors(void); // added
// void status_clearFramReadErrors(void);
void status_incrementFramWriteErrors(void); // added
// void status_clearFramWriteErrors(void);
void status_incrementFramAlignmentErrors(void);
// void status_clearFramAlignmentErrors(void);
void status_framHighWaterMark(uint32_t num_messages); // added
// void status_clearframHighWaterMark(void);

#endif // _STATUS_H_