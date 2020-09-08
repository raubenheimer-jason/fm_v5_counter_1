// FM25V10-G

#include "fram.h"

// // FRAM stuff
// const uint16_t limit_addr_top;
// const uint16_t limit_addr_bottom;

//SRAM opcodes
const uint8_t WREN = 0b00000110;  //set write enable latch
const uint8_t WRDI = 0b00000100;  //write disable
const uint8_t RDSR = 0b00000101;  //read status register
const uint8_t WRSR = 0b00000001;  //write status register
const uint8_t READ = 0b00000011;  //read memory data
const uint8_t WRITE = 0b00000010; //write memory data

const uint8_t PIN_NUM_MISO = 36;
const uint8_t PIN_NUM_MOSI = 35;
const uint8_t PIN_NUM_CLK = 34;
const uint8_t PIN_NUM_CS = 33;

static spi_device_handle_t spi_device;

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

static const char *TAG = "FRAM";

void fram_spi_init(const uint8_t mosi_pin, const uint8_t miso_pin, const uint8_t clk_pin, const uint8_t cs_pin, const uint8_t FRAM_HOST)
{
    // spi_device_handle_t spi_device;

    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing bus SPI...");
    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi_pin,
        .miso_io_num = miso_pin,
        .sclk_io_num = clk_pin,
    };

    //Initialize the SPI bus
    ret = spi_bus_initialize(FRAM_HOST, &buscfg, 0);
    ESP_ERROR_CHECK(ret);

    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 24,
        .mode = 0,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
        .clock_speed_hz = 1 * 1000 * 1000, //Clock out at 10 MHz
        .input_delay_ns = 0,
        .spics_io_num = cs_pin,
        .queue_size = 2, //We want to be able to queue 7 transactions at a time
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    //Attach the FRAM to the SPI bus
    ret = spi_bus_add_device(FRAM_HOST, &devcfg, &spi_device);
    ESP_ERROR_CHECK(ret);
}

void test()
{
    ESP_LOGI(TAG, "---- test function starting ----");
    uint8_t status_reg = read_status_register();
    printf("\n---------------------\n");
    printf("status reg:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_reg));

    uint8_t byte_to_write = 0b00110100;
    uint32_t address_to_write = 100;
    fram_write_byte(address_to_write, byte_to_write);
    printf("\n---------------------\n");
    printf("write byte 0:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(byte_to_write));

    uint8_t test_byte = fram_read_byte(address_to_write);
    printf("\n---------------------\n");
    printf("read byte 0:\t" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test_byte));
}

void cs_init()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (uint64_t)1 << PIN_NUM_CS;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

void cs_low()
{
    gpio_set_level(PIN_NUM_CS, 0);
}

void cs_high()
{
    gpio_set_level(PIN_NUM_CS, 1);
}

// uint8_t read_status_register(const spi_device_handle_t spi_device)
uint8_t read_status_register()
{
    printf("\n--- fram read status register ---\n");

    uint8_t read_byte = 1;
    esp_err_t res;

    spi_transaction_ext_t te;
    memset(&te, 0, sizeof(te));

    te.base.flags = SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA;
    te.address_bits = 0;
    te.base.length = 8 * 2;
    te.base.cmd = RDSR;
    te.base.rxlength = 8;
    printf("t.cmd: %d\n", te.base.cmd);
    res = spi_device_polling_transmit(spi_device, &te);
    printf("read res 1: %d\n", res);
    printf("byte[0]: %d\n", te.base.rx_data[0]);

    read_byte = te.base.rx_data[0];

    return read_byte;
}

// Read byte from fram
// uint8_t fram_read_byte(const spi_device_handle_t spi_device, const uint32_t address)
uint8_t fram_read_byte(const uint32_t address)
{
    printf("\n--- fram read ---\n");

    uint8_t read_byte;
    esp_err_t res;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));

    t.length = 8 * 4;

    t.cmd = READ;

    t.addr = (((address >> 16) | (address >> 8)) | address);

    t.flags = SPI_TRANS_USE_RXDATA;

    t.rxlength = 8;

    printf("here\n");
    res = spi_device_polling_transmit(spi_device, &t);
    printf("read res 1: %d\n", res);

    read_byte = t.rx_data[0];

    return read_byte;
}

// void fram_write_byte(spi_device_handle_t spi_device, uint32_t address, uint8_t data_byte)
void fram_write_byte(uint32_t address, uint8_t data_byte)
{
    printf("\n*** fram write ***\n");

    esp_err_t res;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = 8;

    t.cmd = WREN;

    res = spi_device_polling_transmit(spi_device, &t);
    printf("write res 1: %d\n", res);

    memset(&t, 0, sizeof(t));

    t.cmd = WRITE;
    t.addr = (((address >> 16) | (address >> 8)) | address);

    t.tx_data[0] = data_byte;

    t.length = 8;

    t.flags = SPI_TRANS_USE_TXDATA;

    res = spi_device_polling_transmit(spi_device, &t);
    printf("write res 2: %d\n", res);
}