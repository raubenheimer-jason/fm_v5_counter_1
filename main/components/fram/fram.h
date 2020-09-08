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

void set_bottom(uint32_t bottom_addr);
void set_top(uint32_t top_addr);

#endif // _FRAM_H_