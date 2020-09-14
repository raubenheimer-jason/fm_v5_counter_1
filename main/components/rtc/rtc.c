// DS3231

#include "rtc.h"

// Static prototypes
static uint8_t bcd_to_uint8(uint8_t val);
static uint8_t bcd_to_bin_24h(uint8_t bcdHour);
static void rtc_write_reg(uint8_t reg, uint8_t value);
static uint8_t rtc_read_reg(uint8_t reg);
static esp_err_t i2c_master_init(uint8_t scl_pin, uint8_t sda_pin);
static void rtc_print_register(uint8_t reg_addr);
#if INCLUDE_UINT8_TO_BCD
static uint8_t uint8_to_bcd(uint8_t val);
#endif // INCLUDE_UINT8_TO_BCD

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

/**
 * RTC setup
 */
esp_err_t rtc_begin(uint8_t scl_pin, uint8_t sda_pin)
{
    esp_err_t res = i2c_master_init(scl_pin, sda_pin);
    // could set alarms, etc. here

    ESP_LOGI(TAG, "RTC begin done");

    return res;
}

/**
 * Clear the rtc alarm 2
 */
void rtc_clear_alarm()
{
    // Status register
    uint8_t status_register_value = rtc_read_reg(STATUS_REGISTER_ADDR);

    // clear alarm 2 (BIT 1)
    status_register_value &= ~((uint8_t)1 << 1);

    // write new register value
    rtc_write_reg(STATUS_REGISTER_ADDR, status_register_value); // 0b10001000

    ESP_LOGD(TAG, "cleared alarm 2");
}

/**
 * Configures alarm 2 to trigger every minute (00 seconds of every minute)
 */
void rtc_config_alarm()
{
    rtc_write_reg(ALARM_2_MINUTES_REGISTER_ADDR, 0b10000000);
    rtc_write_reg(ALARM_2_MINUTES_REGISTER_ADDR + 1, 0b10000000);
    rtc_write_reg(ALARM_2_MINUTES_REGISTER_ADDR + 2, 0b10000000);

    // Control register

    // Current value
    uint8_t control_register_value = rtc_read_reg(CONTROL_REGISTER_ADDR);

    control_register_value |= 1UL << 1; // set the first bit (A2IE)

    rtc_write_reg(CONTROL_REGISTER_ADDR, control_register_value); // 0b00011110

    ESP_LOGI(TAG, "config alarm done");
}

/**
 * Print the control register value to the console in binary format
 */
void rtc_print_control_register(void)
{
    rtc_print_register(CONTROL_REGISTER_ADDR);
}

/**
 * Print the status register value to the console in binary format
 */
void rtc_print_status_register(void)
{
    rtc_print_register(STATUS_REGISTER_ADDR);
}

/**
 * Print a single register to the console in binary format
 */
static void rtc_print_register(uint8_t reg_addr)
{
    char reg_val = rtc_read_reg(reg_addr);

    printf("reg [%#08x]: " BYTE_TO_BINARY_PATTERN "\n", reg_addr, BYTE_TO_BINARY(reg_val));
}

#if INCLUDE_RTC_TEST
void rtc_test(void)
{
    ESP_LOGI(TAG, "------------------- RTC test -------------------");

    // uint8_t status_register_value = rtc_read_reg(STATUS_REGISTER_ADDR);
    // printf("status_register_value: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_register_value));

    esp_err_t ret;

    // uint32_t unix_to_set = 1598008483;
    time_t unix_to_set = 1598008483;

    uint32_t unix_from_rtc = 0;

    // ret = rtc_set_date_time(I2C_MASTER_NUM, &unix_to_set);
    ret = rtc_set_date_time(&unix_to_set);

    while (1)
    {
        ESP_LOGI(TAG, "--------------------- RTC test --------------------- ");
        // uint8_t status_reg_val = read_reg_2(STATUS_REGISTER_ADDR);
        uint8_t status_reg_val = rtc_read_reg(STATUS_REGISTER_ADDR);
        printf("status_reg_val: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_reg_val));

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
#endif // INCLUDE_RTC_TEST

/**
 * Read the time from the RTC and return the Unix
 * 
 * @TODO: Check the status register Bit 7: Oscillator Stop Flag (OSF)
 */
uint32_t rtc_get_unix()
{
    // First check the Oscillator Stop Flag (BIT 7 of the Status Regster)
    uint8_t status_register_value = rtc_read_reg(STATUS_REGISTER_ADDR);
    // printf("status_register_value: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_register_value));

    if ((status_register_value >> 7) == 1)
    {
        ESP_LOGW(TAG, "Oscillator Stop Flag == 1, time may be invalid");
        // TODO: Return 0 -------------------------------------------------------------------------------------------------!!!!!!!!!!!!!!!!!!!!!!!!!!!
        return 0;
    }

    uint8_t seconds_reg = 0;
    uint8_t minutes_reg = 0;
    uint8_t hours_reg = 0;
    uint8_t dow_reg = 0;
    uint8_t date_reg = 0;
    uint8_t month_reg = 0;
    uint8_t year_reg = 0;

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

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    uint8_t sec = 0;
    uint8_t min = 0;
    uint8_t hour = 0;
    uint8_t date = 0;
    uint8_t mon = 0;
    uint16_t year = 0;

    if (ret == ESP_OK)
    {
        bool print_info = false;

        if (print_info)
        {
            printf("seconds: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(seconds_reg));
            printf("minutes: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(minutes_reg));
            printf("hours: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(hours_reg));
            printf("day: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(dow_reg));
            printf("date: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(date_reg));
            printf("month: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(month_reg));
            printf("year: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(year_reg));
        }

        sec = bcd_to_uint8(seconds_reg & 0x7F);
        min = bcd_to_uint8(minutes_reg);
        hour = bcd_to_bin_24h(hours_reg);

        date = bcd_to_uint8(date_reg);
        mon = bcd_to_uint8(month_reg & 0x7f);
        year = bcd_to_uint8(year_reg) + 2000;

        if (print_info)
        {
            printf("seconds: %d\n", sec);
            printf("minutes: %d\n", min);
            printf("hours: %d\n", hour);
            printf("date: %d\n", date);
            printf("month: %d\n", mon);
            printf("year: %d\n", year);
        }
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

    ESP_LOGE(TAG, "getting time from rtc probably didn't work...");
    return 0;
}

/**
 * Set RTC date and time from Unix
 * 
 * Also sets the OSF in the status register (0x0F)
 */
// esp_err_t rtc_set_date_time(uint32_t *unix)
esp_err_t rtc_set_date_time(const time_t *unix)
// esp_err_t rtc_set_date_time(uint32_t unix)
{
    struct tm my_time;

    // my_time = *gmtime((time_t *)unix);
    my_time = *gmtime(unix);

    // time values to set
    uint8_t seconds_time = my_time.tm_sec;
    uint8_t minutes_time = my_time.tm_min;
    uint8_t hours_time = my_time.tm_hour;
    // uint8_t dow_time = my_time.tm_wday + 1;
    uint8_t date_time = my_time.tm_mday;
    uint8_t month_time = my_time.tm_mon + 1;
    uint8_t year_time = my_time.tm_year;
    // printf("year: %d\n", year_time);

    // byte values to write to registers
    uint8_t seconds_byte = 0b0000111;
    uint8_t minutes_byte = 0b0001111;
    uint8_t hours_byte = 0b0100111;
    uint8_t dow_byte = 0b00000111; // not using this
    uint8_t date_byte = 0b00001111;
    uint8_t month_byte = 0b00001111;
    uint8_t year_byte = 0b00001111;

    //______________________________________________________________________________________________
    // Set time

    seconds_byte = ((seconds_time / 10) << 4) | (seconds_time % 10);

    minutes_byte = ((minutes_time / 10) << 4) | (minutes_time % 10);

    hours_byte = 1 << 6;                                                               // set 24 hour format
    hours_byte = hours_byte | ((hours_time >= 20 ? 1 : 0) << 5);                       // set 1 if hour is 20-23
    hours_byte = hours_byte | (((hours_time >= 10 && hours_time <= 19) ? 1 : 0) << 4); // set 1 if hour >= 10 and <= 19
    hours_byte = hours_byte | ((hours_time % 20) % 10);                                // value of the last digit

    date_byte = ((date_time / 10) << 4) | (date_time % 10);

    month_byte = ((month_time / 10) << 4) | (month_time % 10);

    year_byte = (((year_time % 100) / 10) << 4) | ((year_time % 100) % 10);

    bool print_info = false;

    if (print_info)
    {
        printf("seconds: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(seconds_byte), seconds_time);
        printf("minutes: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(minutes_byte), minutes_time);
        printf("hours: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(hours_byte), hours_time);
        printf("date: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(date_byte), date_time);
        printf("seconds: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(month_byte), month_time);
        printf("year: " BYTE_TO_BINARY_PATTERN "\t%d\n", BYTE_TO_BINARY(year_byte), year_time);
    }

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

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
        // printf("successfully set rtc time??\n");

        ESP_LOGI(TAG, "update rtc time success!");

        // Clear OSF in status register

        // Status register
        uint8_t status_register_value = rtc_read_reg(STATUS_REGISTER_ADDR);

        // printf("--------------->>  old status register: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_register_value));

        // clear OSF (BIT 7)
        status_register_value &= ~((uint8_t)1 << 7);

        // printf("--------------->>  new status register: " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(status_register_value));

        // write new register value
        rtc_write_reg(STATUS_REGISTER_ADDR, status_register_value);
    }
    else
    {
        ESP_LOGE(TAG, "error updating rtc time");
    }

    return ret;
}

/**
 * Convert BCD to uint8_t
 */
static uint8_t bcd_to_uint8(uint8_t val)
{
    return val - 6 * (val >> 4);
}

#if INCLUDE_UINT8_TO_BCD
/**
 * Convert uint8_t to BCD
 */
static uint8_t uint8_to_bcd(uint8_t val)
{
    return val + 6 * (val / 10);
}
#endif //INCLUDE_UINT8_TO_BCD

/**
 * Convert BCD to binary for 24h time (I think...??)
 */
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

/**
 * Write to a single register
 */
static void rtc_write_reg(uint8_t reg_addr, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "could not write to rtc register");
    }
}

/**
 * Read a single register value
 */
static uint8_t rtc_read_reg(uint8_t reg_addr)
{
    uint8_t value = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    // i2c_master_read_byte(cmd, &value, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1500 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "could not read rtc register (ret = %d)", ret);
    }

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