#ifndef _FRAM_H_
#define _FRAM_H_

// FM25V10-G

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h> /* memset */

void cs_init();
void cs_low();
void cs_high();
uint8_t read_status_register(const spi_device_handle_t spi_device);
uint8_t fram_read_byte(const spi_device_handle_t spi_device, const uint32_t address);
void fram_write_byte(spi_device_handle_t spi_device, uint32_t address, uint8_t data_byte);

#endif // _FRAM_H_