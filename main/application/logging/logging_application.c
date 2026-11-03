#include "logging_application.h"

#include "salvus_logging.h"

#if defined(SALVUS_ESP32_FAMILY_MICROCONTROLLER_SELECTED)
#include "esp_log_application.h"
#endif

salvus_error_type_t logging_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ESP32_FAMILY_MICROCONTROLLER_SELECTED)
    err = esp_log_application_init();
#endif

    return err;
}