#include "status.h"

// static prototypes
static void status_setOnMains(int8_t mains);
static void status_setBatteryChargeStatus(int8_t status);
static void status_setBatteryVoltage(uint32_t voltage);

static const char *TAG = "STATUS";

typedef struct power_status
{
    uint32_t battery_voltage; // Battery voltage in mV (voltage divider already factored in)
    int8_t battery_charge_status;
    int8_t on_mains;
} power_status;

typedef struct wifi_status
{
    uint32_t disconnections;
    int8_t rssi_low_mark;
} wifi_status;

typedef struct mqtt_status
{
    uint32_t upload_success;
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
        .rssi_low_mark = 0,
    },
    .mqtt = {
        .upload_success = 0,
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

// =========================================================== POWER

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
 * Check if the device is running on mains or battery
 * 
 * 1  = device on mains power
 * 0  = device running on battery power
 * -1 = error reading the pin
 */
int8_t status_onMains(void)
{
    uint32_t on_mains_pin_value = gpio_get_level(CONFIG_MAINS_SENSE_PIN);
    if (on_mains_pin_value == 0)
    {
        return 0;
    }
    else if (on_mains_pin_value == 1)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

uint32_t get_battery_voltage(void)
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
    return ((raw_adc_reading * (150 + 2450)) / 8191) * 2; // 13 BITS, x2 for voltage divider, 150 and 2450 not really sure... look at documentation, seems to give most accurate reading
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
    // // Read the current battery voltage
    // int32_t raw_adc_reading = 0;
    // const uint32_t samples = 10;
    // for (uint32_t i = 0; i < samples; i++)
    // {
    //     // ADC setup
    //     adc1_config_width(ADC_WIDTH_BIT_13);
    //     adc1_config_channel_atten(CONFIG_BAT_SENSE_PIN, ADC_ATTEN_DB_11); // ADC1_CHANNEL_9
    //     raw_adc_reading += adc1_get_raw(CONFIG_BAT_SENSE_PIN);            // BAT_SENSE_PIN
    // }
    // raw_adc_reading = raw_adc_reading / samples;
    // uint32_t voltage = ((raw_adc_reading * (150 + 2450)) / 8191) * 2; // 13 BITS, x2 for voltage divider, 150 and 2450 not really sure... look at documentation, seems to give most accurate reading
    // status_setBatteryVoltage(voltage); // 2.090 V // 4178
    status_setBatteryVoltage(get_battery_voltage()); // 2.090 V // 4178

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
    int8_t on_mains = status_onMains();
    if (on_mains == 0)
    {
        status_setOnMains(0);
    }
    else if (on_mains == 1)
    {
        status_setOnMains(1);
    }
    else
    {
        status_setOnMains(-1);
    }
}

// --------------------------------- END POWER

// =========================================================== WIFI

/**
 * Set the RSSI low water mark in the status struct
 * 
 */
void status_setRssiLowWaterMark(int8_t rssi)
{
    if (dev_status.wifi.rssi_low_mark > rssi)
    {
        dev_status.wifi.rssi_low_mark = rssi;
    }
}

/**
 * Increment the wifi disconnections count
 */
void status_incrementWifiDisconnections(void)
{
    dev_status.wifi.disconnections++;
}

// --------------------------------- END WIFI

// =========================================================== MQTT

/**
 * Increment the MQTT upload success count
 */
void status_incrementMqttUploadSuccess(void)
{
    dev_status.mqtt.upload_success++;
}

/**
 * Increment the MQTT upload errors count
 */
void status_incrementMqttUploadErrors(void)
{
    dev_status.mqtt.upload_errors++;
}

/**
 * Increment the MQTT config_read_errors count
 * 
 * Errors if the configuration message could not be parsed
 */
void status_incrementMqttConfigReadErrors(void)
{
    dev_status.mqtt.config_read_errors++;
}

// --------------------------------- END MQTT

// =========================================================== TIME

/**
 * Increment the NTP errors count
 */
void status_incrementNtpErrors(void)
{
    dev_status.system.time.ntp_errors++;
}

/**
 * Increment the system_time_update_errors count
 */
void status_incrementSysTimeUpdateErrors(void)
{
    dev_status.system.time.system_time_update_errors++;
}

/**
 * Increment the rtc_set_time_errors count
 */
void status_incrementRtcSetErrors(void)
{
    dev_status.system.time.rtc_set_time_errors++;
}

/**
 * Increment the rtc_read_errors count
 */
void status_incrementRtcReadErrors(void)
{
    dev_status.system.time.rtc_read_errors++;
}

// --------------------------------- END TIME

// =========================================================== FRAM

/**
 * Increment the FRAM read_errors count
 */
void status_incrementFramReadErrors(void)
{
    dev_status.system.fram.read_errors++;
}

/**
 * Increment the FRAM write_errors count
 */
void status_incrementFramWriteErrors(void)
{
    dev_status.system.fram.write_errors++;
}

/**
 * Increment the FRAM alignment_errors count
 * 
 * Errors where the top and bottom aren't 8 apart etc.
 */
void status_incrementFramAlignmentErrors(void)
{
    dev_status.system.fram.alignment_errors++;
}

/**
 * Set the FRAM high_water_mark
 * 
 * Max number of messages stored in fram
 * 
 * Function checks the current high mark vs the input parameter
 * and updates the status struct if necessary
 */
void status_framHighWaterMark(uint32_t num_messages)
{
    if (num_messages > dev_status.system.fram.high_water_mark)
    {
        dev_status.system.fram.high_water_mark = num_messages;
    }
}

// --------------------------------- END FRAM

/**
 * Need a buffer with a minimum length of 318 bytes
 */
esp_err_t get_status_message_json(char *status_buf)
{

    int spinrtf_ret = sprintf(status_buf, "{\"firm_v\":%s,\"bv\":%d,\"bcs\":%d,\"om\":%d,\"fh\":%d,\"mfh\":%d,\"wd\":%d,\"wr\":%d,\"mue\":%d,\"mcre\":%d,\"ntpe\":%d,\"stue\":%d,\"rste\":%d,\"rre\":%d,\"fre\":%d,\"fwe\":%d,\"fae\":%d,\"fhw\":%d}",
                              CONFIG_FIRMWARE_VERSION,                          // Firm_v
                              dev_status.power.battery_voltage,                 // bv
                              dev_status.power.battery_charge_status,           // bcs
                              dev_status.power.on_mains,                        // om
                              esp_get_free_heap_size(),                         // fh
                              esp_get_minimum_free_heap_size(),                 // mfh
                              dev_status.wifi.disconnections,                   // wd
                              dev_status.wifi.rssi_low_mark,                    // wr
                              dev_status.mqtt.upload_errors,                    // mue
                              dev_status.mqtt.config_read_errors,               // mcre
                              dev_status.system.time.ntp_errors,                // ntpe
                              dev_status.system.time.system_time_update_errors, // stue
                              dev_status.system.time.rtc_set_time_errors,       // rste
                              dev_status.system.time.rtc_read_errors,           // rre
                              dev_status.system.fram.read_errors,               // fre
                              dev_status.system.fram.write_errors,              // fwe
                              dev_status.system.fram.alignment_errors,          // fae
                              dev_status.system.fram.high_water_mark);          // fhw

    if (spinrtf_ret < 0)
    {
        ESP_LOGE(TAG, "spinrtf_ret < 0");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * Prints the status message to the console
 */
void status_printStatusStruct(void)
{
    printf("*************** Status struct ***************\n");

    printf("FIRMWARE VERSION: %s\n", CONFIG_FIRMWARE_VERSION);

    printf("power:\n");
    printf("\tbattery_voltage: %d\n", dev_status.power.battery_voltage);
    printf("\tbattery_charge_status: %d\n", dev_status.power.battery_charge_status);
    printf("\ton_mains: %d\n", dev_status.power.on_mains);

    printf("wifi:\n");
    printf("\tdisconnections: %d\n", dev_status.wifi.disconnections);
    printf("\trssi: %d\n", dev_status.wifi.rssi_low_mark);

    printf("mqtt:\n");
    printf("\tupload_errors: %d\n", dev_status.mqtt.upload_errors);
    printf("\tconfig_read_errors: %d\n", dev_status.mqtt.config_read_errors);

    printf("system:\n");

    printf("\ttime:\n");
    printf("\t\tntp_errors: %d\n", dev_status.system.time.ntp_errors);
    printf("\t\tsystem_time_update_errors: %d\n", dev_status.system.time.system_time_update_errors);
    printf("\t\trtc_set_time_errors: %d\n", dev_status.system.time.rtc_set_time_errors);
    printf("\t\trtc_read_errors: %d\n", dev_status.system.time.rtc_read_errors);

    printf("\tfram:\n");
    printf("\t\tread_errors: %d\n", dev_status.system.fram.read_errors);
    printf("\t\twrite_errors: %d\n", dev_status.system.fram.write_errors);
    printf("\t\talignment_errors: %d\n", dev_status.system.fram.alignment_errors);
    printf("\t\thigh_water_mark: %d\n", dev_status.system.fram.high_water_mark);

    printf("---------------------------------------------\n");
}

/**
 * Resets all the count and water mark variables in the status struct
 */
void status_resetStruct(void)
{
    // wifi
    dev_status.wifi.disconnections = 0;

    // mqtt
    dev_status.mqtt.upload_success = 0;
    dev_status.mqtt.upload_errors = 0;
    dev_status.mqtt.config_read_errors = 0;

    // time
    dev_status.system.time.ntp_errors = 0;
    dev_status.system.time.system_time_update_errors = 0;
    dev_status.system.time.rtc_set_time_errors = 0;
    dev_status.system.time.rtc_read_errors = 0;

    // fram
    dev_status.system.fram.read_errors = 0;
    dev_status.system.fram.write_errors = 0;
    dev_status.system.fram.alignment_errors = 0;
    dev_status.system.fram.high_water_mark = 0;
}