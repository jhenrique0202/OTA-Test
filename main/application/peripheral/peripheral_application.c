#include "peripheral_application.h"

#include "salvus_logging.h"

#include "gpio_application.h"
#include "memory_application.h"
#include "usart_application.h"

static const char *TAG = "PERIPHERAL_APPLICATION";

salvus_error_type_t peripheral_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = memory_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize memory application");
    }

    if (err == SALVUS_ERROR_OK) {
        err = gpio_application_init();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to initialize GPIO application");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = usart_application_init();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to initialize USART application");
        }
    }

    return err;
}

salvus_error_type_t peripheral_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = gpio_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start GPIO application");
    }

    if (err != SALVUS_ERROR_OK) {
        err = usart_application_start();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to start USART application");
        }
    }

    return err;
}
