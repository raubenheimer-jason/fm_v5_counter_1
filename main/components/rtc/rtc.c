// DS3231

#include "rtc.h"

// Static prototypes
static uint8_t bcd_to_uint8(uint8_t val);
static uint8_t uint8_to_bcd(uint8_t val);
static uint8_t bcd_to_bin_24h(uint8_t bcdHour);
static void rtc_write_reg(uint8_t reg, uint8_t value);
static uint8_t rtc_read_reg(uint8_t reg);
static esp_err_t i2c_master_init(uint8_t scl_pin, uint8_t sda_pin);
// static void set_rtc_alarm(uint8_t alarm_to_set);

static const char *TAG = "RTC";

const uint8_t I2C_MASTER_NUM = 1;            /*!< I2C port number for master dev */
const uint32_t I2C_MASTER_FREQ_HZ = 100000;  /*!< I2C master clock frequency */
const uint8_t I2C_MASTER_TX_BUF_DISABLE = 0; /*!< I2C master doesn't need buffer */
const uint8_t I2C_MASTER_RX_BUF_DISABLE = 0; /*!< I2C master doesn't need buffer */

const uint8_t DS3231_SENSOR_ADDR = 0x68; // 0b1101000 /*!< slave address for BH1750 sensor */
const uint8_t WRITE_BIT = 0;             /*!< I2C master write */
const uint8_t READ_BIT = 1;              /*!< I2C master read */
const uint8_t ACK_CHECK_EN = 0x1;        /*!< I2C master will check ack from slave*/
const uint8_t ACK_CHECK_DIS = 0x0;       /*!< I2C master will not check ack from slave */

const uint8_t SECONDS_REGISTER_ADDR = 0x00; /* 00-59 */
const uint8_t MINUTES_REGISTER_ADDR = 0x01; /* 00-59 */
const uint8_t HOURS_REGISTER_ADDR = 0x02;   /* 1-12 + AM/PM   00-23 */
const uint8_t DAY_REGISTER_ADDR = 0x03;     /* 1-7 */
const uint8_t DATE_REGISTER_ADDR = 0x04;    /* 01-31 */
const uint8_t MONTH_REGISTER_ADDR = 0x05;   /* 01-12 + Centry */
const uint8_t YEAR_REGISTER_ADDR = 0x06;    /* 00-99 */

const uint8_t ALARM_2_MINUTES_REGISTER_ADDR = 0x0B; /*  */
const uint8_t CONTROL_REGISTER_ADDR = 0x0E;         /*  */
const uint8_t STATUS_REGISTER_ADDR = 0x0F;          /*  */

void rtc_print_alarm_registers(void)
{
    uint8_t status_reg_addr = 0x0B;
    uint8_t test = rtc_read_reg(status_reg_addr);
    printf("0x0B: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test));

    status_reg_addr += 1;
    test = rtc_read_reg(status_reg_addr);
    printf("0x0C: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test));

    status_reg_addr += 1;
    test = rtc_read_reg(status_reg_addr);
    printf("0x0D: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test));
}

void rtc_reset_alarm(uint8_t alarm_to_clear)
{
    // first need to read status register
    int ret;
    i2c_cmd_handle_t cmd1 = i2c_cmd_link_create();

    // Start
    i2c_master_start(cmd1);

    // Master write slave address + write (0) [need to write register pointer, where to read from]
    i2c_master_write_byte(cmd1, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Send address from where to read
    i2c_master_write_byte(cmd1, STATUS_REGISTER_ADDR, ACK_CHECK_EN);

    // Ack from slave

    // Repeated start
    i2c_master_start(cmd1);

    // Master write slave address + read (1)
    i2c_master_write_byte(cmd1, DS3231_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Master read data from slave
    uint8_t status_reg_value;
    i2c_master_read_byte(cmd1, &status_reg_value, I2C_MASTER_ACK);

    // Ack from master

    // .... repeat until all necessary registers have been read

    // NACK from master

    // STOP from master
    i2c_master_stop(cmd1);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd1, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd1);

    // then clear the desired alarm bit

    printf("status register: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_reg_value));

    status_reg_value &= ~((uint8_t)1 << (alarm_to_clear - 1)); // clearing bit 0 or 1

    printf("new status register: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_reg_value));

    i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();

    // Start
    i2c_master_start(cmd2);

    // Master write slave address + write (0) [need to write register pointer, where to read from]
    i2c_master_write_byte(cmd2, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Send address from where to read
    i2c_master_write_byte(cmd2, STATUS_REGISTER_ADDR, ACK_CHECK_EN);

    // Ack from slave

    // Repeated start
    i2c_master_start(cmd2);

    // Master write slave address + read (1)
    i2c_master_write_byte(cmd2, DS3231_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Master read data from slave
    i2c_master_read_byte(cmd2, &status_reg_value, I2C_MASTER_ACK);

    // Ack from master

    // .... repeat until all necessary registers have been read

    // NACK from master

    // STOP from master
    i2c_master_stop(cmd2);

    printf("here??\n");

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd2, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd2);

    printf("function done??\n");
}

esp_err_t rtc_begin(uint8_t scl_pin, uint8_t sda_pin)
{
    // Init I2C
    i2c_master_init(scl_pin, sda_pin);

    // Set alarm registers to trigger interrupt every minute
    // set_rtc_alarm(2);
    ESP_LOGI(TAG, "RTC begin done");

    return ESP_OK;
}

void rtc_print_status_register(void)
{
    const uint8_t status_reg_addr = 0x0F;

    char test = rtc_read_reg(status_reg_addr);

    printf("status register: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(test));
}

/**
 * Sets the RTC alarm to trigger the interrupt once per minute (00 seconds of every minute)
 */
// static esp_err_t set_rtc_alarm(void)
// static void set_rtc_alarm(uint8_t alarm_to_set)
void set_rtc_alarm(uint8_t alarm_to_set)
{
    // Set ALARM 2 REGISTER MASK BITS (BIT 7)
    // 0Bh (A2M2 = 1)
    // 0Ch (A2M3 = 1)
    // 0Dh (A2M4 = 1)

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Master start
    i2c_master_start(cmd);

    // Master send slave address + write (0)
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Slave send ACK

    // Master send address
    i2c_master_write_byte(cmd, ALARM_2_MINUTES_REGISTER_ADDR, ACK_CHECK_EN);

    // Slave send ACK

    // Master send data

    uint8_t alarm_2_byte = 0b10000000; // only intrested in setting BIT 7 of each register

    i2c_master_write_byte(cmd, alarm_2_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, alarm_2_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, alarm_2_byte, ACK_CHECK_EN);

    // Slave send ACK

    // .... repeat

    // Master send stop
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
        printf("successfully set rtc alarm 2 registers\n");
    }

    // Need  to set the control register
    cmd = i2c_cmd_link_create();

    // Master start
    i2c_master_start(cmd);

    // Master send slave address + write (0)
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Slave send ACK

    // Master send address
    i2c_master_write_byte(cmd, CONTROL_REGISTER_ADDR, ACK_CHECK_EN);

    // Slave send ACK

    // Master send data

    uint8_t control_register_value = 0b00011100 | (1 << (alarm_to_set - 1));

    printf("control_register_value__: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(control_register_value));

    i2c_master_write_byte(cmd, control_register_value, ACK_CHECK_EN);

    // Slave send ACK

    // .... repeat

    // Master send stop
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
        printf("successfully set rtc control register\n");
    }

    rtc_reset_alarm(alarm_to_set);
}

void rtc_test(void)
{
    int ret;

    uint32_t unix_to_set = 1598008483;

    uint32_t unix_from_rtc = 0;

    // ret = rtc_set_date_time(I2C_MASTER_NUM, &unix_to_set);
    ret = rtc_set_date_time(&unix_to_set);

    while (1)
    {
        ESP_LOGI(TAG, "RTC test");
        // unix_from_rtc = rtc_get_unix(I2C_MASTER_NUM); //, &unix_from_rtc);
        unix_from_rtc = rtc_get_unix(); //, &unix_from_rtc);

        printf("rtc_unix: %d\n", unix_from_rtc);

        if (ret == ESP_ERR_TIMEOUT)
        {
            ESP_LOGE(TAG, "I2C Timeout");
        }
        else if (ret == ESP_OK)
        {

            printf("*******************\n");
            printf("ret == ESP_OK\n");
            printf("*******************\n");
        }
        else
        {
            ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

/**
 * Read the time from the RTC and return the Unix
 * 
 * @TODO: Check the status register Bit 7: Oscillator Stop Flag (OSF)
 */
// uint32_t rtc_get_unix(i2c_port_t i2c_num) //, uint32_t *unix)
uint32_t rtc_get_unix() //, uint32_t *unix)
{
    // uint8_t *seconds = NULL;
    // uint8_t *minutes = NULL;
    // uint8_t *hours = NULL;
    // uint8_t *day = NULL;
    // uint8_t *date = NULL;
    // uint8_t *month = NULL;
    // uint8_t *year = NULL;

    uint8_t seconds_reg = 0;
    uint8_t minutes_reg = 0;
    uint8_t hours_reg = 0;
    uint8_t dow_reg = 0;
    uint8_t date_reg = 0;
    uint8_t month_reg = 0;
    uint8_t year_reg = 0;

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start
    i2c_master_start(cmd);

    // Master write slave address + write (0) [need to write register pointer, where to read from]
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Send address from where to read
    i2c_master_write_byte(cmd, SECONDS_REGISTER_ADDR, ACK_CHECK_EN);

    // Ack from slave

    // Repeated start
    // i2c_master_write_byte(cmd, I2C_CMD_RESTART, ACK_CHECK_EN);
    i2c_master_start(cmd);

    // Master write slave address + read (1)
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);

    // Ack from slave

    // Master read data from slave
    i2c_master_read_byte(cmd, &seconds_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &minutes_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &hours_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &dow_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &date_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &month_reg, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &year_reg, I2C_MASTER_NACK);

    // Ack from master

    // .... repeat until all necessary registers have been read

    /*
        Addresses for date time:

        0x00 - Seconds          00-59
        0x01 - Minutes          00-59 
        0x02 - Hours            1-12 + AM/PM   00-23
        0x03 - Day              1-7
        0x04 - Date             01-31
        0x05 - Month/ Centry    01-12 + Centry
        0x06 - Year             00-99

    */

    // NACK from master

    // STOP from master
    i2c_master_stop(cmd);

    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    uint8_t sec = 0;
    uint8_t min = 0;
    uint8_t hour = 0;
    uint8_t date = 0;
    uint8_t mon = 0;
    uint16_t year = 0;

    if (ret == ESP_OK)
    {
        printf("DID IT WORK???\n");

        /*
            uint8_t *seconds = NULL;
            uint8_t *minutes = NULL;
            uint8_t *hours = NULL;
            uint8_t *day = NULL;
            uint8_t *date = NULL;
            uint8_t *month = NULL;
            uint8_t *year = NULL;

        */

        printf("seconds: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(seconds_reg));
        printf("minutes: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(minutes_reg));
        printf("hours: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(hours_reg));
        printf("day: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(dow_reg));
        printf("date: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(date_reg));
        printf("month: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(month_reg));
        printf("year: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(year_reg));

        sec = bcd_to_uint8(seconds_reg & 0x7F);
        min = bcd_to_uint8(minutes_reg);
        hour = bcd_to_bin_24h(hours_reg);

        date = bcd_to_uint8(date_reg);
        mon = bcd_to_uint8(month_reg & 0x7f);
        year = bcd_to_uint8(year_reg) + 2000;

        printf("seconds: %d\n", sec);
        printf("minutes: %d\n", min);
        printf("hours: %d\n", hour);
        printf("date: %d\n", date);
        printf("month: %d\n", mon);
        printf("year: %d\n", year);

        struct tm my_time;

        // time values to set
        my_time.tm_sec = sec;
        my_time.tm_min = min;
        my_time.tm_hour = hour;
        my_time.tm_mday = date;
        my_time.tm_mon = mon - 1;
        my_time.tm_year = year - 1900;

        return mktime(&my_time);
    }
    else
    {
        printf("probably didnt work... ");
    }

    return 0;
}

// esp_err_t rtc_set_date_time(i2c_port_t i2c_num, uint32_t *unix)
esp_err_t rtc_set_date_time(uint32_t *unix)
{
    // *unix = 1597999965;
    struct tm my_time;

    my_time = *gmtime((time_t *)unix);

    // printf("sec: %d\n", my_time.tm_sec);
    // printf("min: %d\n", my_time.tm_min);
    // printf("hour: %d\n", my_time.tm_hour);
    // printf("date: %d\n", my_time.tm_mday);
    // printf("month: %d\n", my_time.tm_mon);
    // printf("year: %d\n", my_time.tm_year);
    // printf("dow: %d\n", my_time.tm_wday);

    // return ESP_OK;

    // time values to set
    uint8_t seconds_time = my_time.tm_sec;
    uint8_t minutes_time = my_time.tm_min;
    uint8_t hours_time = my_time.tm_hour;
    uint8_t dow_time = my_time.tm_wday + 1;
    uint8_t date_time = my_time.tm_mday;
    uint8_t month_time = my_time.tm_mon + 1;
    uint8_t year_time = my_time.tm_year;
    printf("year: %d\n", year_time);

    // byte values to write to registers
    uint8_t seconds_byte = 0b0000111;
    uint8_t minutes_byte = 0b0001111;
    uint8_t hours_byte = 0b0100111;
    uint8_t dow_byte = 0b00000111; // not using this
    uint8_t date_byte = 0b00001111;
    uint8_t month_byte = 0b00001111;
    uint8_t year_byte = 0b00001111;

    /*
            SET TIME !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    */

    seconds_byte = ((seconds_time / 10) << 4) | (seconds_time % 10);
    printf("seconds: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(seconds_byte), seconds_time);

    minutes_byte = ((minutes_time / 10) << 4) | (minutes_time % 10);
    printf("minutes: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(minutes_byte), minutes_time);

    hours_byte = 1 << 6;                                                               // set 24 hour format
    hours_byte = hours_byte | ((hours_time >= 20 ? 1 : 0) << 5);                       // set 1 if hour is 20-23
    hours_byte = hours_byte | (((hours_time >= 10 && hours_time <= 19) ? 1 : 0) << 4); // set 1 if hour >= 10 and <= 19
    hours_byte = hours_byte | ((hours_time % 20) % 10);                                // value of the last digit
    printf("hours: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(hours_byte), hours_time);

    date_byte = ((date_time / 10) << 4) | (date_time % 10);
    printf("date: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(date_byte), date_time);

    month_byte = ((month_time / 10) << 4) | (month_time % 10);
    printf("seconds: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(month_byte), month_time);

    year_byte = (((year_time % 100) / 10) << 4) | ((year_time % 100) % 10);
    printf("year: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(year_byte), year_time);

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Master start
    i2c_master_start(cmd);

    // Master send slave address + write (0)
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);

    // Slave send ACK

    // Master send address
    i2c_master_write_byte(cmd, SECONDS_REGISTER_ADDR, ACK_CHECK_EN);

    // Slave send ACK

    // Master send data
    i2c_master_write_byte(cmd, seconds_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, minutes_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, hours_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, dow_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, date_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, month_byte, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, year_byte, ACK_CHECK_EN);

    // Slave send ACK

    // .... repeat

    // Master send stop
    i2c_master_stop(cmd);

    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
        printf("successfully set rtc time??\n");
    }

    return ret;
}

static uint8_t bcd_to_uint8(uint8_t val)
{
    return val - 6 * (val >> 4);
}

static uint8_t uint8_to_bcd(uint8_t val)
{
    return val + 6 * (val / 10);
}

static uint8_t bcd_to_bin_24h(uint8_t bcdHour)
{
    uint8_t hour;
    if (bcdHour & 0x40)
    {
        // 12 hour mode, convert to 24
        bool isPm = ((bcdHour & 0x20) != 0);

        hour = bcd_to_uint8(bcdHour & 0x1f);
        if (isPm)
        {
            hour += 12;
        }
    }
    else
    {
        hour = bcd_to_uint8(bcdHour);
    }
    return hour;
}

// static void rtc_write_reg(i2c_port_t i2c_num, uint8_t address, uint8_t reg, uint8_t value)
// static void rtc_write_reg(uint8_t address, uint8_t reg, uint8_t value)
static void rtc_write_reg(uint8_t reg, uint8_t value)
{
    // i2c_start(dsBus);
    // i2c_writeByte(dsBus, RTC_Write);
    // i2c_writeByte(dsBus, reg);
    // i2c_writeByte(dsBus, value);
    // i2c_stop(dsBus);

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "reg_write error");
    // }

    // if (ret == ESP_OK)
    // {
    //     printf("write success\n");
    // }
    // else
    // {
    //     printf("write ret: NOT OK\n");
    // }
}

// static uint8_t rtc_read_reg(i2c_port_t i2c_num, uint8_t address, uint8_t reg)
// static uint8_t rtc_read_reg(uint8_t address, uint8_t reg)
static uint8_t rtc_read_reg(uint8_t reg)
{
    uint8_t value = 0;

    // i2c_start(dsBus);
    // i2c_writeByte(dsBus, RTC_Write);
    // i2c_writeByte(dsBus, reg);
    // i2c_start(dsBus);
    // i2c_writeByte(dsBus, RTC_Read);
    // value = i2c_readByte(dsBus, 1);
    // i2c_stop(dsBus);

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    int i = 0;
    // printf("here %d\n", i++);

    // Start
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    // i2c_master_write_byte(cmd, I2C_CMD_RESTART, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &value, I2C_MASTER_ACK);
    i2c_master_stop(cmd);

    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    // printf("here %d\n", i++);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1500 / portTICK_RATE_MS);
    // printf("here %d\n", i++);

    i2c_cmd_link_delete(cmd);
    // printf("here %d\n", i++);

    // if (ret == ESP_OK)
    // {
    //     printf("read success\n");
    // }
    return value;
}

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(uint8_t scl_pin, uint8_t sda_pin)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl_pin;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}