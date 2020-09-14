#ifndef _MAIN_H_
#define _MAIN_H_

// extern bool rtc_alarm_flag; // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files
extern xQueueHandle fram_store_queue;

// extern xSemaphoreHandle rtc_alarm_flag_gatekeeper;
extern SemaphoreHandle_t rtc_alarm_flag_gatekeeper;

// extern char device_id[20];

#endif // _MAIN_H_