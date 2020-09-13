#include "status.h"

// static prototypes
static void status_setOnMains(int8_t mains);
static void status_setBatteryChargeStatus(int8_t status);
static void status_setBatteryVoltage(uint32_t voltage);

typedef struct power_status
{
    uint32_t battery_voltage;
    int8_t battery_charge_status;
    int8_t on_mains;
} power_status;

typedef struct wifi_status
{
    uint32_t disconnections;
    int8_t rssi;
} wifi_status;

typedef struct mqtt_status
{
    uint32_t upload_errors;
    uint32_t config_read_errors;
} mqtt_status;

typedef struct time_status
{
    uint32_t ntp_errors;
    uint32_t system_time_update_errors;
    uint32_t rtc_set_time_errors;
    uint32_t rtc_read_errors;
} time_status;

typedef struct fram_status
{
    uint32_t read_errors;
    uint32_t write_errors;
    uint32_t alignment_errors;
    uint32_t high_water_mark;
} fram_status;

typedef struct system_status
{
    time_status time;
    fram_status fram;
} system_status;

typedef struct device_status
{
    power_status power;
    wifi_status wifi;
    mqtt_status mqtt;
    system_status system;
} device_status;

static device_status dev_status = {
    .power = {
        .battery_voltage = 0,
        .battery_charge_status = false,
        .on_mains = false,
    },
    .wifi = {
        .disconnections = 0,
        .rssi = 0,
    },
    .mqtt = {
        .upload_errors = 0,
        .config_read_errors = 0,
    },
    .system = {
        .time = {
            .ntp_errors = 0,
            .system_time_update_errors = 0,
            .rtc_set_time_errors = 0,
            .rtc_read_errors = 0,
        },
        .fram = {
            .read_errors = 0,
            .write_errors = 0,
            .alignment_errors = 0,
            .high_water_mark = 0,
        },
    },
};

/**
 * Sets the battery voltage in the status struct
 */
static void status_setBatteryVoltage(uint32_t voltage)
{
    dev_status.power.battery_voltage = voltage;
}

/**
 * Sets the battery charge status in the status struct
 * 
 * 1  = battery not charging (either full or no mains power)
 * 0  = battery busy charging
 * -1 = error reading the pin
 */
static void status_setBatteryChargeStatus(int8_t status)
{
    dev_status.power.battery_charge_status = status;
}

/**
 * Sets the bool indicating whether the device 
 * is on mains or battery power in the status struct
 * 
 * 1  = device on mains power
 * 0  = device running on battery power
 * -1 = error reading the pin
 */
static void status_setOnMains(int8_t mains)
{
    dev_status.power.on_mains = mains;
}

/**
 * Checks:
 *  - battery voltage
 *  - battery charge status
 *  - on mains or on battery
 * 
 * MUST INITIALISE GPIOS BEFORE CALLING THIS FUNCTION
 */
void status_evaluatePower(void)
{
    // Read the current battery voltage
    int32_t raw_adc_reading = 0;
    const uint32_t samples = 10;
    for (uint32_t i = 0; i < samples; i++)
    {
        // ADC setup
        adc1_config_width(ADC_WIDTH_BIT_13);
        adc1_config_channel_atten(CONFIG_BAT_SENSE_PIN, ADC_ATTEN_DB_11); // ADC1_CHANNEL_9
        raw_adc_reading += adc1_get_raw(CONFIG_BAT_SENSE_PIN);            // BAT_SENSE_PIN
    }
    raw_adc_reading = raw_adc_reading / samples;
    uint32_t voltage = ((raw_adc_reading * (150 + 2450)) / 8191) * 2; // 13 BITS, x2 for voltage divider, 150 and 2450 not really sure... look at documentation, seems to give most accurate reading
    status_setBatteryVoltage(voltage);                                // 2.090 V // 4178

    // Check the battery charge status
    uint32_t status_pin_value = gpio_get_level(CONFIG_BAT_STATUS_PIN);
    if (status_pin_value == 0)
    {
        status_setBatteryChargeStatus(0);
    }
    else if (status_pin_value == 1)
    {
        status_setBatteryChargeStatus(1);
    }
    else
    {
        status_setBatteryChargeStatus(-1);
    }

    // Check if the device is on mains power or on battery
    uint32_t on_mains_pin_value = gpio_get_level(CONFIG_MAINS_SENSE_PIN);
    if (on_mains_pin_value == 0)
    {
        status_setOnMains(0);
    }
    else if (on_mains_pin_value == 1)
    {
        status_setOnMains(1);
    }
    else
    {
        status_setOnMains(-1);
    }
}

/**
 * Allocate memory on the heap for the status message
 * 
 * MUST FREE MEMORY AFTER USE
 */
// char *get_status_message(void)
// {
// }

/**
 * Prints the status message to the console
 */
void status_printStatusMessage(void)
{
    printf("*************** Status struct ***************\n");
    printf("power:\n");
    printf("\tbattery_voltage: %d\n", dev_status.power.battery_voltage);
    printf("\tbattery_charge_status: %d\n", dev_status.power.battery_charge_status);
    printf("\ton_mains: %d\n", dev_status.power.on_mains);
    printf("---------------------------------------------\n");
}