#include "gpio.h"

// Static prototypes
static void IRAM_ATTR rtc_alarm_isr(void *arg);
static void IRAM_ATTR counter_isr(void *arg);
static esp_err_t gpio_interrupt_init(void);
static esp_err_t gpio_leds_init(void);

static uint32_t count = 0;

static const char *TAG = "GPIO";

// Semaphore for count variable
// static xSemaphoreHandle count_gatekeeper = 0;
// static SemaphoreHandle_t count_gatekeeper = 0;
// static SemaphoreHandle_t count_gatekeeper = NULL;
static xSemaphoreHandle count_gatekeeper = NULL;

esp_err_t gpio_initial_setup(void)
{
    // initialise semaphore
    count_gatekeeper = xSemaphoreCreateMutex();
    // count_gatekeeper = xSemaphoreCreateBinary();

    if (count_gatekeeper == NULL)
    {
        ESP_LOGE(TAG, "count_gatekeeper == NULL");
        return ESP_FAIL;
    }

    //install gpio isr service
    esp_err_t isr_install_ret = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    if (isr_install_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "isr_install_ret != ESP_OK");
        return isr_install_ret;
    }

    esp_err_t isr_init = gpio_interrupt_init();
    if (isr_init != ESP_OK)
    {
        ESP_LOGE(TAG, "isr_init != ESP_OK");
        return isr_init;
    }

    esp_err_t led_init_ret = gpio_leds_init();
    if (led_init_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "led_init_ret != ESP_OK");
        return led_init_ret;
    }

    // if (on_mains_flag == 1) // only turn the LED on if device is on mains power
    // {
    //     printf("turning led on ---------------------------------------------------------------------------------------------------------------------------------------\n");
    //     gpio_set_level(CONFIG_WIFI_LED_PIN, 1);
    // }
    // gpio_set_level(CONFIG_MQTT_LED_PIN, 0);

    ESP_LOGI(TAG, "GPIO init done");
    return ESP_OK;
}

static void IRAM_ATTR rtc_alarm_isr(void *arg)
{
    // if (xSemaphoreTake(count_gatekeeper, 200))
    if (xSemaphoreTake(count_gatekeeper, 200) == pdTRUE)
    // if (xSemaphoreTakeFromISR(count_gatekeeper, 200) == pdTRUE)
    // if (xSemaphoreTakeFromISR(count_gatekeeper, NULL) == pdTRUE)
    {
        // store count value in local variable
        uint32_t local_count = count;

        // clear count
        count = 0;

        // release count
        xSemaphoreGive(count_gatekeeper);
        // xSemaphoreGiveFromISR(count_gatekeeper, NULL);

        // if (xSemaphoreTake(rtc_alarm_flag_gatekeeper, 100)) // old
        // if (xSemaphoreTake(rtc_alarm_flag_gatekeeper, 100) == pdTRUE)
        // if (xSemaphoreTakeFromISR(rtc_alarm_flag_gatekeeper, 100) == pdTRUE) // old
        // if (xSemaphoreTakeFromISR(rtc_alarm_flag_gatekeeper, NULL) == pdTRUE) // old
        // {
            // rtc_alarm_flag = true;
            // xSemaphoreGive(rtc_alarm_flag_gatekeeper);
            // xSemaphoreGiveFromISR(rtc_alarm_flag_gatekeeper, NULL);
        // }
        // send count and time data to Fram Task

        // get time from esp32
        time_t unix_now;
        time(&unix_now);

        // store time and count in uint64_t
        uint64_t telemetry = (uint64_t)unix_now << 32 | local_count;

        // send telemetry to fram_queue
        xQueueSendFromISR(fram_store_queue, &telemetry, NULL);
    }
}

static void IRAM_ATTR counter_isr(void *arg)
{
    // if (xSemaphoreTake(count_gatekeeper, 200))
    if (xSemaphoreTake(count_gatekeeper, 200) == pdTRUE)
    // if (xSemaphoreTakeFromISR(count_gatekeeper, 200) == pdTRUE)
    // if (xSemaphoreTakeFromISR(count_gatekeeper, NULL) == pdTRUE)
    {
        count++;
        xSemaphoreGive(count_gatekeeper); // release count
        // xSemaphoreGiveFromISR(count_gatekeeper, NULL); // release count
    }
}

/**
 * Initialise the gpio interrupts
 * (RTC alarm & counter)
 */
static esp_err_t gpio_interrupt_init(void)
{
    gpio_config_t gpio_interrupt_pin_config = {
        .pin_bit_mask = GPIO_INPUT_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    // esp_err_t ret = ESP_OK;

    esp_err_t config_ret = gpio_config(&gpio_interrupt_pin_config);
    if (config_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "config_ret != ESP_OK");
        return config_ret;
    }

    // Change the interrupt type for the counter pin
    esp_err_t intr_type_ret = gpio_set_intr_type(CONFIG_COUNTER_PIN, GPIO_INTR_POSEDGE);
    if (intr_type_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "CONFIG_COUNTER_PIN intr_type_ret != ESP_OK");
        return intr_type_ret;
    }

    // Remove interrupts from battery charge status pin
    intr_type_ret = gpio_set_intr_type(CONFIG_BAT_STATUS_PIN, GPIO_INTR_DISABLE);
    if (intr_type_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "CONFIG_BAT_STATUS_PIN intr_type_ret != ESP_OK");
        return intr_type_ret;
    }

    // Remove interrupts from mains sense pin
    intr_type_ret = gpio_set_intr_type(CONFIG_MAINS_SENSE_PIN, GPIO_INTR_DISABLE);
    if (intr_type_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "CONFIG_MAINS_SENSE_PIN intr_type_ret != ESP_OK");
        return intr_type_ret;
    }

    // hook isr handlers for specific gpio pins
    esp_err_t handler_ret = gpio_isr_handler_add(CONFIG_RTC_ALARM_PIN, rtc_alarm_isr, (void *)CONFIG_RTC_ALARM_PIN);
    if (handler_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "rtc_alarm_isr handler_ret != ESP_OK");
        return handler_ret;
    }
    handler_ret = gpio_isr_handler_add(CONFIG_COUNTER_PIN, counter_isr, (void *)CONFIG_COUNTER_PIN);
    if (handler_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "counter_isr handler_ret != ESP_OK");
        return handler_ret;
    }

    return ESP_OK;
}

/**
 * Initialise the LED gpios
 * (red and blue LED)
 */
static esp_err_t gpio_leds_init(void)
{
    gpio_config_t led_gpios_config = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_BITMASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&led_gpios_config);
}