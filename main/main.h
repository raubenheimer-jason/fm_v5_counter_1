#ifndef _MAIN_H_
#define _MAIN_H_

// #include <stdint.h>

// Counter
// uint32_t count = 0;

extern bool rtc_alarm_flag; // https://stackoverflow.com/questions/1045501/how-do-i-share-variables-between-different-c-files
extern xQueueHandle fram_store_queue;

#endif // _MAIN_H_