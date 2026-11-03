#include "memory_application.h"

#include "salvus_logging.h"

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
#include "esp32s3_nvm_application.h"
#endif

static const char *TAG = "MEMORY_APPLICATION";

salvus_error_type_t
memory_application_get_nvm_handle(salvus_hal_nvm_handle_t *handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_nvm_application_get_handle(handle);
#endif

    return err;
}

salvus_error_type_t memory_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
    err = esp32s3_nvm_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize ESP32S3 NVM application");
    }
#endif

    return err;
}
