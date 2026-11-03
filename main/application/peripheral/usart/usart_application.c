#include "usart_application.h"

#include "salvus_logging.h"

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
#include "esp32s3_usart_application.h"
#endif

static const char *TAG = "USART_APPLICATION";

salvus_error_type_t
usart_application_get_usart_handle(usart_application_handle_type_t type,
                                   salvus_hal_usart_handle_t *usart_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_usart_application_get_usart_handle(type, usart_handle);
#endif

    return err;
}

salvus_error_type_t usart_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_usart_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize ESP32S3 USART application");
    }
#endif

    return err;
}

salvus_error_type_t usart_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_usart_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start ESP32S3 USART application");
    }
#endif

    return err;
}