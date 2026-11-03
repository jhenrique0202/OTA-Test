#include "esp32s3_gpio_application.h"

#include "salvus_logging.h"

#include "salvus_hal_esp_gpio_types.h"
#include "salvus_hal_gpio.h"

#include "peripheral_application.h"

static const char *TAG = "ESP32S3_GPIO_APPLICATION";

static salvus_error_type_t _init_gpio()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_gpio_handle_t gpio_handle = NULL;
    err = salvus_hal_gpio_get_handle(SALVUS_HAL_ESP_GPIO, 0, &gpio_handle);

    if (err != SALVUS_ERROR_OK) {
        salvus_hal_esp_gpio_config_t gpio_config = {
            .base_config = {
                .type = SALVUS_HAL_ESP_GPIO,
                .unit = 0,
            }};

        err = salvus_hal_gpio_init(&gpio_config, &gpio_handle);
    }

    return err;
}

static salvus_error_type_t
_init_output_pin(uint32_t pin, salvus_hal_pin_level_type_t initial_level,
                 salvus_hal_pin_handle_t *pin_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_gpio_handle_t gpio_handle = NULL;
    err = salvus_hal_gpio_get_handle(SALVUS_HAL_ESP_GPIO, 0, &gpio_handle);

    if (err == SALVUS_ERROR_OK) {
        err = salvus_hal_gpio_get_pin_handle(gpio_handle, 0, pin, pin_handle);

        if (err != SALVUS_ERROR_OK) {
            salvus_hal_esp_pin_config_t pin_config = {
                .base_config = {.port = 0,
                                .pin = pin,
                                .initial_level = initial_level},
                .gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                .pull_up_en = GPIO_PULLUP_DISABLE,
                                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                .intr_type = GPIO_INTR_DISABLE}};

            err =
                salvus_hal_gpio_init_pin(gpio_handle, &pin_config, pin_handle);
        }
    }

    return err;
}

static salvus_error_type_t _init_input_pin(uint32_t pin,
                                           gpio_pullup_t pull_up_en,
                                           gpio_pulldown_t pull_down_en,
                                           gpio_int_type_t interrupt_type,
                                           salvus_hal_pin_handle_t *pin_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_gpio_handle_t gpio_handle = NULL;
    err = salvus_hal_gpio_get_handle(SALVUS_HAL_ESP_GPIO, 0, &gpio_handle);

    if (err == SALVUS_ERROR_OK) {
        err = salvus_hal_gpio_get_pin_handle(gpio_handle, 0, pin, pin_handle);

        if (err != SALVUS_ERROR_OK) {
            salvus_hal_esp_pin_config_t pin_config = {
                .base_config = {.port = 0, .pin = pin},
                .gpio_config = {.mode = GPIO_MODE_INPUT,
                                .pull_up_en = pull_up_en,
                                .pull_down_en = pull_down_en,
                                .intr_type = interrupt_type}};

            err =
                salvus_hal_gpio_init_pin(gpio_handle, &pin_config, pin_handle);
        }
    }

    return err;
}

static salvus_error_type_t _init_pins()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_pin_handle_t pin_handle = NULL;

    err =
        _init_input_pin(COMMUNICATION_MODULE_STATUS_PIN, GPIO_PULLUP_DISABLE,
                        GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE, &pin_handle);
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG,
                    "Failed to initialize communication module status pin");
    }

#if defined(SALVUS_ENABLE_SIMCOM_MODULE_RESET_PIN)
    if (err == SALVUS_ERROR_OK) {
        pin_handle = NULL;

        err = _init_output_pin(COMMUNICATION_MODULE_RESET_PIN,
                               SALVUS_HAL_PIN_LOW, &pin_handle);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG,
                        "Failed to initialize communication module reset pin");
        }
    }
#endif

    if (err == SALVUS_ERROR_OK) {
        pin_handle = NULL;

        err = _init_output_pin(COMMUNICATION_MODULE_POWER_ENABLE_PIN,
                               SALVUS_HAL_PIN_LOW, &pin_handle);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(
                TAG,
                "Failed to initialize communication module power enable pin");
        }
    }

    return err;
}

salvus_error_type_t
esp32s3_gpio_application_get_pin_handle(gpio_application_pin_handle_type_t type,
                                        salvus_hal_pin_handle_t *pin_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_gpio_handle_t gpio_handle = NULL;
    err = salvus_hal_gpio_get_handle(SALVUS_HAL_ESP_GPIO, 0, &gpio_handle);

    if (err == SALVUS_ERROR_OK) {
        switch (type) {
        case GPIO_APPLICATION_COMMUNICATION_MODULE_STATUS_PIN_HANDLE :
            err = salvus_hal_gpio_get_pin_handle(
                gpio_handle, 0, COMMUNICATION_MODULE_STATUS_PIN, pin_handle);
            break;

#if defined(SALVUS_ENABLE_SIMCOM_MODULE_RESET_PIN)
        case GPIO_APPLICATION_COMMUNICATION_MODULE_RESET_PIN_HANDLE :
            err = salvus_hal_gpio_get_pin_handle(
                gpio_handle, 0, COMMUNICATION_MODULE_RESET_PIN, pin_handle);
            break;
#endif

        case GPIO_APPLICATION_COMMUNICATION_MODULE_POWER_ENABLE_PIN_HANDLE :
            err = salvus_hal_gpio_get_pin_handle(
                gpio_handle, 0, COMMUNICATION_MODULE_POWER_ENABLE_PIN,
                pin_handle);
            break;

        default :
            break;
        }
    }

    return err;
}

salvus_error_type_t esp32s3_gpio_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = _init_gpio();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize GPIO");
    }

    if (err == SALVUS_ERROR_OK) {
        err = _init_pins();
    }

    return err;
}

salvus_error_type_t esp32s3_gpio_application_start() { return SALVUS_ERROR_OK; }