#ifndef _FRAM_H_
#define _FRAM_H_

// FM25V10-G

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h> /* memset */
#include "esp_log.h"

#define INCLUDE_TEST 0

void fram_spi_init();

#if INCLUDE_TEST
void test();
#endif // INCLUDE_TEST

uint32_t get_stored_messages_count();

bool delete_last_read_telemetry(uint64_t d_to_del);
uint64_t read_telemetry();
bool write_telemetry(uint64_t telemetry);

void fram_reset();

bool check_state();

void display_top_bottom();

#endif // _FRAM_H_