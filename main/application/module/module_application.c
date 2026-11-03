#include "module_application.h"

#include "salvus_logging.h"

#include "simcom_application.h"

static const char *TAG = "MODULE_APPLICATION";

salvus_error_type_t module_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = simcom_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize SIMCom application");
    }

    return err;
}

salvus_error_type_t module_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = simcom_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start SIMCom application");
    }

    return err;
}