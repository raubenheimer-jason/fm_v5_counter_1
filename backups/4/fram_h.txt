#ifndef _FRAM_H_
#define _FRAM_H_

// FM25V10-G

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h> /* memset */
#include "esp_log.h"

// void fram_spi_init(const uint8_t mosi_pin, const uint8_t miso_pin, const uint8_t clk_pin, const uint8_t cs_pin, const uint8_t FRAM_HOST);
void fram_spi_init();
void test();

uint32_t get_stored_messages_count();

bool delete_last_read_telemetry(uint64_t d_to_del);
uint64_t read_telemetry();
bool write_telemetry(uint64_t telemetry);

void fram_reset();

bool check_state();

void display_top_bottom();

#endif // _FRAM_H_