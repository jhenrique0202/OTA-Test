#include "gpio_application.h"

#include "salvus_logging.h"

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
#include "esp32s3_gpio_application.h"
#endif

static const char *TAG = "GPIO_APPLICATION";

salvus_error_type_t
gpio_application_get_pin_handle(gpio_application_pin_handle_type_t type,
                                salvus_hal_pin_handle_t *pin_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_gpio_application_get_pin_handle(type, pin_handle);
#endif

    return err;
}

salvus_error_type_t gpio_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_gpio_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize ESP32S3 GPIO application");
    }
#endif

    return err;
}

salvus_error_type_t gpio_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_gpio_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start ESP32S3 GPIO application");
    }
#endif

    return err;
}