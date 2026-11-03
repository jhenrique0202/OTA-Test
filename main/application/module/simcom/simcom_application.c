#include "simcom_application.h"

#include "salvus_logging.h"

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
#include "y70xx_application.h"
#endif

static const char *TAG = "SIMCOM_APPLICATION";

salvus_error_type_t
simcom_application_get_simcom_handle(salvus_simcom_handle_t *simcom_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_get_simcom_handle(simcom_handle);
#endif

    return err;
}

salvus_error_type_t simcom_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize Y70XX application");
    }
#endif

    return err;
}

salvus_error_type_t simcom_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_start();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start Y70XX application");
    }
#endif

    return err;
}

salvus_error_type_t simcom_application_turn_on()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_turn_on();
#endif

    return err;
}

salvus_error_type_t simcom_application_turn_off()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_turn_off();
#endif

    return err;
}

salvus_error_type_t simcom_application_check_power_status(uint8_t *status)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_check_power_status(status);
#endif

    return err;
}

salvus_error_type_t simcom_application_configure_network_connection()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_configure_network_connection();
#endif

    return err;
}

salvus_error_type_t simcom_application_establish_network_connection()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_establish_network_connection();
#endif

    return err;
}

salvus_error_type_t simcom_application_check_connection_status(uint8_t *status)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
    err = y70xx_application_check_connection_status(status);
#endif

    return err;
}