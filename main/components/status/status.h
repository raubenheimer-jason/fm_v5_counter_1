#ifndef _STATUS_H_
#define _STATUS_H_

#include <stdint.h>
#include "esp_system.h"

#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/adc.h"

// Prototypes
void status_resetStruct(void);
esp_err_t get_status_message_json(char *status_buf);
void status_printStatusStruct(void);

// power
int8_t status_onMains(void);
void status_evaluatePower(void);
uint32_t get_battery_voltage(void);
// wifi
void status_setRssiLowWaterMark(int8_t rssi);  // added
void status_incrementWifiDisconnections(void); // added
// mqtt
void status_incrementMqttUploadSuccess(void); // added
void status_incrementMqttUploadErrors(void);  // added
void status_incrementMqttConfigReadErrors(void);
// time
void status_incrementNtpErrors(void);
void status_incrementSysTimeUpdateErrors(void);
void status_incrementRtcSetErrors(void);
void status_incrementRtcReadErrors(void);
// fram
void status_incrementFramReadErrors(void);  // added
void status_incrementFramWriteErrors(void); // added
void status_incrementFramAlignmentErrors(void);
void status_framHighWaterMark(uint32_t num_messages); // added

#endif // _STATUS_H_