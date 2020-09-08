#ifndef _FRAM_H_
#define _FRAM_H_

// FM25V10-G

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h> /* memset */
#include "esp_log.h"

void cs_init();
void cs_low();
void cs_high();
// uint8_t read_status_register(const spi_device_handle_t spi_device);
// uint8_t fram_read_byte(const spi_device_handle_t spi_device, const uint32_t address);
// void fram_write_byte(spi_device_handle_t spi_device, uint32_t address, uint8_t data_byte);
uint8_t read_status_register();
uint8_t fram_read_byte(const uint32_t address);
void fram_write_byte(uint32_t address, uint8_t data_byte);

void fram_spi_init(const uint8_t mosi_pin, const uint8_t miso_pin, const uint8_t clk_pin, const uint8_t cs_pin, const uint8_t FRAM_HOST);
void test();

#endif // _FRAM_H_