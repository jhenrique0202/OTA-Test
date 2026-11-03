#include "application.h"

#include "salvus_logging.h"

#include "ota_application.h"
#include "logging_application.h"
#include "module_application.h"
#include "peripheral_application.h"

static const char *TAG = "APPLICATION";

salvus_error_type_t application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = logging_application_init();

    if (err == SALVUS_ERROR_OK) {
        err = peripheral_application_init();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to initialize peripheral application");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = module_application_init();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to initialize module application");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = ota_application_init();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to initialize OTA application");
        }
    }

    return err;
}

salvus_error_type_t application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = peripheral_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start peripheral application");
    }

    if (err == SALVUS_ERROR_OK) {
        err = module_application_start();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to start module application");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = ota_application_start();
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to start OTA application");
        }
    }

    return err;
}
