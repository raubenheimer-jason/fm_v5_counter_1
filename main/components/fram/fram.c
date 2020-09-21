// FM25V10-G

#include "fram.h"

// --------------------------- FRAM CONFIG ---------------------------
// pins
const uint8_t SPI_MOSI_PIN = 36;
const uint8_t SPI_MISO_PIN = 34;
const uint8_t SPI_CLK_PIN = 35;
const uint8_t SPI_CS_PIN = 33;
// host
const uint8_t FRAM_HOST = SPI2_HOST;
// data sizes etc.
const uint8_t telemetry_bytes = 8;
// ((128,000-16)/ 8) == 15,998 -- > so memory address limit top = 128,000 <-- doesn't really make sense? // ((8000-8)/ 8) == 999 --> so memory address limit top = 8000-8 = 7992
const uint32_t limit_addr_top = 128000; // (15998x8 + 16)
const uint8_t limit_addr_bottom = 8;    // using 32 bit addresses now

// --------------------------- FRAM CONFIG ---------------------------

//SRAM opcodes
const uint8_t WREN = 0b00000110;  //set write enable latch
const uint8_t WRDI = 0b00000100;  //write disable
const uint8_t RDSR = 0b00000101;  //read status register
const uint8_t WRSR = 0b00000001;  //write status register
const uint8_t READ = 0b00000011;  //read memory data
const uint8_t WRITE = 0b00000010; //write memory data

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

static uint32_t top;
static uint32_t bottom;

// This is only used for testing (but don't want to delete the function)
#define INCLUDE_READ_STATUS_REGISTER 0

// ======================================= Static function prototypes =======================================

#if INCLUDE_READ_STATUS_REGISTER
static uint8_t read_status_register();
#endif // INCLUDE_READ_STATUS_REGISTER
static uint8_t fram_read_byte(const uint32_t address);
static void fram_write_byte(const uint32_t address, uint8_t data_byte);
static void set_top(uint32_t top_addr);
static void set_bottom(uint32_t bottom_addr);
static void get_top_bottom();
static void fram_internal_setup();

/**
 * Initialises the SPI bus for the FRAM
 * 
 * Also calls "fram_internal_setup()"
 */
void fram_spi_init()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing FRAM SPI...");
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_CLK_PIN,
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
        .clock_speed_hz = 15 * 1000 * 1000, //Clock out at 10 MHz
        .input_delay_ns = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 2, //We want to be able to queue 7 transactions at a time
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    //Attach the FRAM to the SPI bus
    ret = spi_bus_add_device(FRAM_HOST, &devcfg, &spi_device);
    ESP_ERROR_CHECK(ret);

    fram_internal_setup();
}

void display_top_bottom()
{
    printf("top = %d, bottom = %d\n", top, bottom);
}

#if INCLUDE_TEST
void test()
{
    /*
    uint32_t not_0 = 0;
    for (uint32_t i = 0; i < 128000; i++)
    {
        uint8_t val = fram_read_byte(i);
        if (val != 0)
        {
            not_0++;
        }
        // printf("val = %d    addr = %d\n", val, i);
    }

    printf("\nnot 0: %d\n", not_0);
    */
    /*
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
    */
}
#endif // INCLUDE_TEST

/**
 * Writes telemetry to memory (at the top)
 * 
 * Then updates top memory value (locally and on the fram)
 */
bool write_telemetry(uint64_t telemetry)
{
    if (((bottom == limit_addr_bottom) && (top == (limit_addr_top - limit_addr_bottom))) || (top == (bottom - telemetry_bytes)))
    {
        ESP_LOGW(TAG, "memory full, return");
        return false; // the memory is full (minus 8 bytes... cant make it where top == bottom because that could also mean empty)
    }

    // need to write 8 bytes to memory (64 bit number)
    uint8_t shift = 56;
    for (uint32_t i = top; i < (top + telemetry_bytes); i++) // start writing at position "top", MSB first
    {
        uint8_t b = (uint8_t)(telemetry >> shift);
        fram_write_byte(i, b);
        shift -= telemetry_bytes;
    }

    uint32_t new_top = top + telemetry_bytes;

    if (new_top > limit_addr_top)
    {
        new_top = limit_addr_bottom; // this is fine because we have already done the check to see if it's full
    }

    // Already checked if new_top will be valid at the top of the function
    set_top(new_top); // update on fram and locally

    return true;
}

/**
 * Reads the next avaliable telemetry (from "bottom" position)
 * 
 * Then updates bottom memory value (locally and on the fram) DOES IT REALLY ??? I DONT THINK SO
 * 
 * If there is no telemetry to read, it returns 0
 */
uint64_t read_telemetry()
{
    if (bottom == top)
    {
        // Nothing to read?
        return 0;
    }

    uint64_t telemetry = 0;

    for (uint32_t i = bottom; i < (bottom + 8); i++)
    {
        uint8_t b = fram_read_byte(i);
        telemetry = (telemetry << 8) | (uint64_t)b;
    }

    return telemetry;
}

/**
 * Basically just moves the "bottom" position up 8 positions
 * 
 * need to check that the bottom position < top position (what about going over the memory limit... special case need to check) 
 * 
 * @arg d_to_del: the data to delete from fram (the telemetry)
 */
bool delete_last_read_telemetry(uint64_t d_to_del)
{
    if (bottom == top)
    {
        ESP_LOGW(TAG, "memory is empty, nothing to delete");
        return false; // memory is empty
    }

    // need to check that the intended data is deleted - dont just delete "blind"
    if (read_telemetry() == d_to_del)
    {
        // Intended data to delete is in the next position in fram - continue the delete
        uint32_t new_bottom = bottom + telemetry_bytes;

        if (new_bottom > limit_addr_top) // it will never be = to. --> Are we sure with the new FRAM?
        {
            new_bottom = limit_addr_bottom;
        }

        set_bottom(new_bottom); // already protected from "overtaking" the top by returning if they are equal...
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Returns the current number of stored telemetry messages
 */
uint32_t get_stored_messages_count()
{
    uint32_t diff = 0;

    uint32_t local_top = top;
    uint32_t local_bottom = bottom;

    if (local_top > local_bottom)
    {
        diff = local_top - local_bottom;
    }
    else if (local_bottom > local_top)
    {
        diff = (limit_addr_top - local_bottom) + (local_top - limit_addr_bottom);
    }
    else if (local_bottom == local_top)
    {
        diff = 0;
    }

    uint32_t message_num = diff / telemetry_bytes; // integer division ??

    return message_num;
}

/**
 * Sets the top and bottom at the lower limit
 * Used to correct fram if there is an error (basically if top and bottom address's arent at least 8 apart etc)
 * Also used the first time to initialise fram ??
 */
void fram_reset()
{
    ESP_LOGW(TAG, "resetting FRAM (setting: top = bottom = limit_addr_bottom = %d)", limit_addr_bottom);
    set_top(limit_addr_bottom);
    set_bottom(limit_addr_bottom);
}

/**
 * Checks that the top and bottom addresses are 8 apart (or multiples of (addr - bottom) / telemetry size)
 */
bool check_state()
{
    ESP_LOGI(TAG, "checking state: top = %d, bottom = %d", top, bottom);
    // this checks that the top and bottom addresses (- bottom limit) are divisible by 8
    // if each are individually divisible by 8 (when bottom limit is - from the address), then they must be a multple of 8 away from each other
    if ((bottom - limit_addr_bottom) % telemetry_bytes != 0 || (top - limit_addr_bottom) % telemetry_bytes != 0)
    {
        ESP_LOGE(TAG, "error with address values (bottom: %d  top: %d)", bottom, top);
        return false;
    }
    else
    {
        return true;
    }
}

// Private ------------------------------------------------------------

/**
 * Basic setup for FRAM
 * 
 * get_top_bottom() <-- reads the first 8 bytes and sets the local "top" and "bottom" variables
 * check_state()    <-- Basic checks for errors, calls "fram_reset()" if there are any errors
 *                          - fram_reset()  <-- sets "top" and "bottom" = limit_addr_bottom
 */
static void fram_internal_setup()
{
    get_top_bottom();

    if (check_state() == false)
    {
        fram_reset();
    }
    ESP_LOGI(TAG, "internal setup complete");
}

#if INCLUDE_READ_STATUS_REGISTER
/**
 * Read the status register
 * Expected result: 0b01000000
 */
static uint8_t read_status_register()
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
    res = spi_device_polling_transmit(spi_device, &te.base);
    printf("read res 1: %d\n", res);
    printf("byte[0]: %d\n", te.base.rx_data[0]);

    read_byte = te.base.rx_data[0];

    return read_byte;
}
#endif // INCLUDE_READ_STATUS_REGISTER

/**
 * Read single byte from FRAM at the specified address
 */
static uint8_t fram_read_byte(const uint32_t address)
{
    uint8_t read_byte;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = 8 * 4;
    t.cmd = READ;
    t.addr = address;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.rxlength = 8;
    spi_device_polling_transmit(spi_device, &t);
    read_byte = t.rx_data[0];

    return read_byte;
}

/**
 * Write single byte to FRAM at the specified address
 */
static void fram_write_byte(const uint32_t address, uint8_t data_byte)
{
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.cmd = WREN;
    spi_device_polling_transmit(spi_device, &t);
    memset(&t, 0, sizeof(t));
    t.cmd = WRITE;
    t.addr = address;
    t.tx_data[0] = data_byte;
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(spi_device, &t);
}

/**
 * Set the top address in FRAM and local variable
 */
static void set_top(uint32_t top_addr)
{
    uint8_t b0 = (uint8_t)(top_addr >> 24); // MSB  (bit 31 -> 24)
    uint8_t b1 = (uint8_t)(top_addr >> 16); //      (bit 23 -> 16)
    uint8_t b2 = (uint8_t)(top_addr >> 8);  //      (bit 15 -> 8)
    uint8_t b3 = (uint8_t)top_addr;         // LSB  (bit 7 --> 0)

    fram_write_byte(4, b0);
    fram_write_byte(5, b1);
    fram_write_byte(6, b2);
    fram_write_byte(7, b3);

    top = top_addr;
}

/**
 * Set the bottom address in FRAM and local variable
 */
static void set_bottom(uint32_t bottom_addr)
{
    uint8_t b0 = (uint8_t)(bottom_addr >> 24); // MSB   (bit 31 --> 24)
    uint8_t b1 = (uint8_t)(bottom_addr >> 16); //       (bit 23 --> 16)
    uint8_t b2 = (uint8_t)(bottom_addr >> 8);  //       (bit 15 --> 8)
    uint8_t b3 = (uint8_t)bottom_addr;         // LSB   (bit 7  --> 0)

    fram_write_byte(0, b0);
    fram_write_byte(1, b1);
    fram_write_byte(2, b2);
    fram_write_byte(3, b3);

    bottom = bottom_addr;
}

/**
 * Gets the first 8 bytes of memory
 * 
 * The first 4 makes the bottom position
 * The 5th to the 7th (inclusive) make the top position
 * 
 * Sets the local "top" and "bottom" variables
 */
static void get_top_bottom()
{
    ESP_LOGI(TAG, "getting top bottom");

    // bottom
    uint8_t b0 = fram_read_byte(0);
    uint8_t b1 = fram_read_byte(1);
    uint8_t b2 = fram_read_byte(2);
    uint8_t b3 = fram_read_byte(3);

    // top
    uint8_t b4 = fram_read_byte(4);
    uint8_t b5 = fram_read_byte(5);
    uint8_t b6 = fram_read_byte(6);
    uint8_t b7 = fram_read_byte(7);

    bottom = (uint32_t)b0 << 24;
    bottom |= b1 << 16;
    bottom |= b2 << 8;
    bottom |= b3;

    top = (uint32_t)b4 << 24;
    top |= b5 << 16;
    top |= b6 << 8;
    top |= b7;

    // TODO: If top or bottom are either lower than lower limit or higher than higher limit (other checks as well like having 8 bytes in between bottom of bottom and top) -- then just set top = bottom = lower limit ?? make sure we dont wipe memory by mistake...

    ESP_LOGI(TAG, "top: %d   bottom: %d", top, bottom);
}